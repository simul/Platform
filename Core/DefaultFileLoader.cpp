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


#if PLATFORM_STD_FILESYSTEM==0
#define SIMUL_FILESYSTEM 0
#elif PLATFORM_STD_FILESYSTEM==1
#define SIMUL_FILESYSTEM 1
#include <filesystem>
namespace fs = std::filesystem;
#else
#define SIMUL_FILESYSTEM PLATFORM_STD_FILESYSTEM
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
static int nextslash(const std::string &str,int pos)
{
	int slash=(int)str.find('/',pos);
	int back=(int)str.find('\\',pos);
	if(slash<0||(back>=0&&back<slash))
		slash=back;
	return slash;
}
static int mkpath(const std::string &filename_utf8)
{
    int status = 0;
	int pos=0;
    while (status == 0 && (pos = nextslash(filename_utf8,pos))>=0)
    {
		status = do_mkdir(filename_utf8.substr(0,pos).c_str());
		pos++;
    }
    return (status);
}

DefaultFileLoader::DefaultFileLoader()
{
}

#ifdef _MSC_VER
#pragma optimize("",off)
#endif
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


bool DefaultFileLoader::Save(const void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)
{
	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
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

void DefaultFileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text)
{
	if(!FileExists(filename_utf8))
	{
		SIMUL_CERR<<"Failed to find file "<<filename_utf8<<std::endl;
		pointer=NULL;
		return;
	}
	
	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
#if defined(_MSC_VER) && defined(_WIN32)
	_wfopen_s(&fp,wstr.c_str(),L"rb");//open_as_text?L"r":L"rb")
#else
	fp = fopen(filename_utf8,"rb");//r, ccs=UTF-8
#endif
	if(!fp)
	{
		std::cerr<<"Not a file: "<<filename_utf8<<std::endl;
		pointer=NULL;
		return;
	}
	
	fseek(fp, 0, SEEK_END);
	bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pointer = malloc(bytes+1);
	fread(pointer, 1, bytes, fp);
	if(open_as_text)
		((char*)pointer)[bytes]=0;
	
	fclose(fp);
	if(recordFilesLoaded)
		filesLoaded.insert(filename_utf8);
}

static double GetDayNumberFromDateTime(int year,int month,int day,int hour,int min,int sec)
{
    int D = 367*year - (7*(year + ((month+9)/12)))/4 + (275*month)/9 + day - 730531;//was +2451545
	double d=(double)D;
	d+=(double)hour/24.0;
	d+=(double)min/24.0/60.0;
	d+=(double)sec/24.0/3600.0;
	return d;
}

double DefaultFileLoader::GetFileDate(const char* filename_utf8) const
{
	if(!FileExists(filename_utf8))
		return 0.0;

	std::wstring wstr=platform::core::Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
#if defined(_MSC_VER) && defined(_WIN32)
	_wfopen_s(&fp,wstr.c_str(),L"rb");//open_as_text?L"r, ccs=UTF-8":
#else
	fp = fopen(filename_utf8,"rb");//open_as_text?L"r, ccs=UTF-8":
#endif
	if(!fp)
	{
		//std::cerr<<"Failed to find file "<<filename_utf8<<std::endl;
		return 0.0;
	}
	fclose(fp);

#if defined(_MSC_VER) && defined(_WIN32)
	struct _stat buf;
	_wstat(wstr.c_str(),&buf);
	buf.st_mtime;
	time_t t = buf.st_mtime;
	struct tm lt;
	gmtime_s(&lt,&t);
	// Note: bizarrely, the tm structure has MONTHS starting at ZERO, but DAYS start at 1.
	double daynum=GetDayNumberFromDateTime(1900+lt.tm_year,lt.tm_mon+1,lt.tm_mday,lt.tm_hour,lt.tm_min,lt.tm_sec);
	return daynum;
#elif SIMUL_FILESYSTEM
#ifdef CPP20
	auto write_time=fs::last_write_time(filename_utf8);
	const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
	const auto time = std::chrono::system_clock::to_time_t(systemTime);
	return ((double)ns)/(3600.0*24.0*1000000.0);
#else
	std::wstring filenamew=StringToWString(filename_utf8);
	struct stat buf;
	stat(filename_utf8, &buf);
	buf.st_mtime;
	time_t t = buf.st_mtime;
	struct tm lt;
#if __COMMODORE__
	gmtime_s(&t,&lt);
#else
	gmtime_r(&t,&lt);
#endif
	double datetime=GetDayNumberFromDateTime(1900+lt.tm_year,lt.tm_mon,lt.tm_mday,lt.tm_hour,lt.tm_min,lt.tm_sec);
	return datetime;
#endif
#else
	return 0;
#endif
}

void DefaultFileLoader::ReleaseFileContents(void* pointer)
{
	free(pointer);
}

static FileLoader *fileLoader = nullptr;

FileLoader *FileLoader::GetFileLoader()
{
	if (!fileLoader)
	{
		static DefaultFileLoader defaultFileLoader = DefaultFileLoader();
		fileLoader = &defaultFileLoader;
	}
	return fileLoader;
}

void FileLoader::SetFileLoader(FileLoader *f)
{
	fileLoader=f;
}
