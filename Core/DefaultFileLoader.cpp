#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>
#if defined(_MSC_VER) && defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#endif
#include <stdio.h> // for fopen, seek, fclose
#include <stdlib.h> // for malloc, free
// TODO: replace stdlib.h with cstdlib
#include <time.h>
typedef struct stat Stat;
using namespace platform;
using namespace core;

#if PLATFORM_STD_FILESYSTEM == 1
#include <filesystem>
namespace fs = std::filesystem;
#elif PLATFORM_STD_FILESYSTEM == 2
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

static int do_mkdir(const char *path_utf8)
{
	ALWAYS_ERRNO_CHECK
	int             status = 0;
	// TO-DO: this
#ifndef NN_NINTENDO_SDK
#ifdef _MSC_VER
	struct _stat64i32            st;
	std::wstring wstr=platform::core::Utf8ToWString(path_utf8);
	if (_wstat(wstr.c_str(), &st) != 0)
#else
	Stat            st;
	if (stat(path_utf8, &st)!=0)
#endif
	{
		/* Directory does not exist. EEXIST for race condition */
#ifdef _MSC_VER
		if (_wmkdir(wstr.c_str()) != 0 && errno != EEXIST)
#else
		if (mkdir(path_utf8,S_IRWXU) != 0 && errno != EEXIST)
#endif
			status = -1;
	}
	else if (!(st.st_mode & S_IFDIR))
	{
		//errno = ENOTDIR;
		status = -1;
	}
#endif

	errno=0;
	return(status);
}
static int mkpath(const std::string &filename_utf8)
{
	int status = 0;
	int pos = 0;
	while (status == 0 && (pos = FileLoader::NextSlash(filename_utf8, pos)) >= 0)
	{
		status = do_mkdir(filename_utf8.substr(0, pos).c_str());
		pos++;
	}
	return (status);
}


DefaultFileLoader::DefaultFileLoader()
{
}

bool DefaultFileLoader::FileExists(const char *filename_utf8) const
{
	enum access_mode
	{
		NO_FILE=-1,EXIST=0,WRITE=2,READ=4
	};
#if defined(_MSC_VER) && defined(_WIN32)
	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	access_mode res=(access_mode)_waccess(wstr.c_str(),READ);
	bool bExists = (res!=NO_FILE);
	if(errno==EACCES)
	{
		SIMUL_CERR<<"DefaultFileLoader::FileExists Access denied: for "<<filename_utf8<<" the file's permission setting does not allow specified access."<<std::endl;
	}
	else if(errno==EINVAL)
	{
		SIMUL_CERR<<"DefaultFileLoader::FileExists Invalid parameter: for "<<filename_utf8<<".\n";
	}
	else if(errno==ENOENT)
	{
		// ENOENT: File name or path not found.
		// This is an acceptable error as we are just checking whether the file exists.
	}
	// If res is NO_FILE, errno will now be set, so we must clear it.
	errno=0;
#elif NN_NINTENDO_SDK
	// TO-DO: this
	bool bExists = false;
#elif __COMMODORE__
	bool bExists = fs::exists(filename_utf8);
#else
	Stat st;
	bool bExists=(stat(filename_utf8, &st)==0);
	errno=0;
#endif
	return bExists;
}

void DefaultFileLoader::AcquireFileContents(void *&pointer, unsigned int &bytes, const char *filename_utf8, bool open_as_text)
{
	if (!FileExists(filename_utf8))
	{
		SIMUL_CERR << "Failed to find file " << filename_utf8 << std::endl;
		pointer = nullptr;
		return;
	}

	std::wstring wstr = platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = nullptr;
#if defined(_MSC_VER) && defined(_WIN32)
	_wfopen_s(&fp, wstr.c_str(), L"rb"); // open_as_text?L"r":L"rb")
#else
	fp = fopen(filename_utf8, "rb"); // r, ccs=UTF-8
#endif
	if (!fp)
	{
		std::cerr << "Not a file: " << filename_utf8 << std::endl;
		pointer = nullptr;
		return;
	}

	fseek(fp, 0, SEEK_END);
	bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pointer = malloc(bytes + 1);
	fread(pointer, 1, bytes, fp);
	if (open_as_text)
		((char *)pointer)[bytes] = 0;

	fclose(fp);
	if (recordFilesLoaded)
		filesLoaded.insert(filename_utf8);
}

uint64_t DefaultFileLoader::GetFileDate(const char *filename_utf8) const
{
	if (!FileExists(filename_utf8))
		return 0;

	std::wstring wstr = platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = nullptr;
#if defined(_MSC_VER) && defined(_WIN32)
	_wfopen_s(&fp, wstr.c_str(), L"rb"); // open_as_text?L"r, ccs=UTF-8":
#else
	fp = fopen(filename_utf8, "rb"); // open_as_text?L"r, ccs=UTF-8":
#endif
	if (!fp)
	{
		// std::cerr<<"Failed to find file "<<filename_utf8<<std::endl;
		return 0;
	}
	fclose(fp);

#if 0//PLATFORM_STD_FILESYSTEM
	// https://stackoverflow.com/questions/61030383/how-to-convert-stdfilesystemfile-time-type-to-time-t
	// https://stackoverflow.com/questions/51273205/how-to-compare-time-t-and-stdfilesystemfile-time-type
	fs::file_time_type fileTime = fs::last_write_time(filename_utf8);
#if PLATFORM_CXX20
	const std::chrono::system_clock systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
	const time_t time = std::chrono::system_clock::to_time_t(systemTime);
#else
	std::chrono::system_clock::time_point systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
	const time_t time = std::chrono::system_clock::to_time_t(systemTime);
#endif
	return time;
#endif

#if defined(_MSC_VER) && defined(_WIN32)
	struct _stat buf;
	_wstat(wstr.c_str(), &buf);
	time_t t = buf.st_mtime;
	return t;
#else
	std::wstring filenamew = StringToWString(filename_utf8);
	struct stat buf;
	stat(filename_utf8, &buf);
	time_t t = buf.st_mtime;
	return t;
#endif
	return 0;
}

bool DefaultFileLoader::Save(const void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)
{
	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = nullptr;
	// Ensure the directory exists:
	{
		mkpath(filename_utf8);
	}
	
#if defined(_MSC_VER) && defined(_WIN32)
	_wfopen_s(&fp,wstr.c_str(),L"wb");//w, ccs=UTF-8
#else
	fp = fopen(filename_utf8,"wb");//w, ccs=UTF-8
#endif
	
	if(!fp)
	{
		std::cerr<<"Failed to open file "<<filename_utf8<<std::endl;
		return false;
	}
	fseek(fp, 0, SEEK_SET);
	fwrite(pointer, 1, bytes, fp);
	if(save_as_text)
	{
		char c=0;
		fwrite(&c, 1, 1, fp);
	}
	fclose(fp);
	
	return true;
}

void DefaultFileLoader::ReleaseFileContents(void *pointer)
{
	free(pointer);
}
