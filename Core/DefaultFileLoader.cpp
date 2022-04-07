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
#include <time.h>
typedef struct stat Stat;
using namespace simul;
using namespace base;
using namespace platform;
using namespace core;

#if defined(__ANDROID__)
AAssetManager* DefaultFileLoader::s_AssetManager = nullptr;
#else defined(__SWITCH__)
#include <nn/fs.h>
std::string DefaultFileLoader::s_NVN_ROM_Name;
#endif


#if PLATFORM_STD_FILESYSTEM==0
#define SIMUL_FILESYSTEM 0
#elif PLATFORM_STD_FILESYSTEM==1
#define SIMUL_FILESYSTEM 1
#include <filesystem>
namespace fs = std::filesystem;
#else
#define SIMUL_FILESYSTEM 1
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

static int do_mkdir(const char *path_utf8)
{
	ALWAYS_ERRNO_CHECK
    int             status = 0;

#if defined(__SWITCH__)
	nn::Result res = nn::fs::CreateDirectory(path_utf8);
	if (res.IsFailure())
	{
		SIMUL_CERR << "Failed to create directory: " << path_utf8 << std::endl;
		status = -1;
	}
	return status;
#endif

#if defined(_MSC_VER) && defined(_WIN32)
    struct _stat64i32            st;
	std::wstring wstr=platform::core::Utf8ToWString(path_utf8);
    if (_wstat(wstr.c_str(), &st) != 0)
#else
    Stat            st;
    if (stat(path_utf8, &st)!=0)
#endif
    {
        /* Directory does not exist. EEXIST for race condition */
#if defined(_MSC_VER) && defined(_WIN32)
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

std::vector<std::string> FileLoader::ListDirectory(const std::string &path) const
{
	std::vector<std::string> dir;
#if SIMUL_FILESYSTEM
	try
	{
		if(!fs::exists(path))
		{
			SIMUL_COUT<<"path does not exist: "<<path.c_str()<<std::endl;
			return dir;
		}
		if(!fs::is_directory(path))
		{
			SIMUL_COUT<<"path is not directory: "<<path.c_str()<<std::endl;
			return dir;
		}
		for (const auto & entry : fs::directory_iterator(path))
			dir.push_back(entry.path().string());
	}
	catch(...)
	{
		SIMUL_COUT<<"ListDirectory failed for path "<<path.c_str()<<std::endl;
	}
#else
#endif
	return dir;
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
		std::cerr<<"DefaultFileLoader::FileExists Access denied: for "<<filename_utf8<<" the file's permission setting does not allow specified access."<<std::endl;
	}
	else if(errno==EINVAL)
	{
		std::cerr<<"DefaultFileLoader::FileExists Invalid parameter: for "<<filename_utf8<<".\n";
	}
	else if(errno==ENOENT)
	{
		// ENOENT: File name or path not found.
		// This is an acceptable error as we are just checking whether the file exists.
	}
	// If res is NO_FILE, errno will now be set, so we must clear it.
	errno=0;
#elif __SWITCH__
	bool bExists = false;
	nn::fs::DirectoryEntryType type;
	std::string filepath = s_NVN_ROM_Name + std::string(filename_utf8);
	nn::Result res = nn::fs::GetEntryType(&type, filepath.c_str());
	if (res.IsSuccess())
		bExists = type == nn::fs::DirectoryEntryType::DirectoryEntryType_File;
#elif __COMMODORE__
	bool bExists = fs::exists(filename_utf8);
#elif __ANDROID__
	AAsset* asset = AAssetManager_open(s_AssetManager, filename_utf8, AASSET_MODE_UNKNOWN);
	bool bExists = (asset != nullptr);
	if (asset)
		AAsset_close(asset);
#else
    Stat st;
    bool bExists=(stat(filename_utf8, &st)==0);
	errno=0;
#endif
	return bExists;
}

std::string FileLoader::FindFileInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const
{
	const char **paths=new const char *[path_stack_utf8.size()+1];
	for(size_t i=0;i<path_stack_utf8.size();i++)
	{
		paths[i]=path_stack_utf8[i].c_str();
	}
	paths[path_stack_utf8.size()]=nullptr;
	
	std::string f= FindFileInPathStack(filename_utf8,paths);
	delete [] paths;
	return f;
}

int FileLoader::FindIndexInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const
{
	const char **paths=new const char *[path_stack_utf8.size()+1];
	for(size_t i=0;i<path_stack_utf8.size();i++)
	{
		paths[i]=path_stack_utf8[i].c_str();
	}
	paths[path_stack_utf8.size()]=nullptr;
	int i= FindIndexInPathStack(filename_utf8,paths);
	delete [] paths;
	return i;
}

std::string FileLoader::FindFileInPathStack(const char *filename_utf8, const char* const* path_stack_utf8) const
{
	int s=FindIndexInPathStack(filename_utf8,path_stack_utf8);
	if(s==-1)
		return filename_utf8;
	if(s<-1)
		return "";
	return std::string(path_stack_utf8[s])+std::string("/")+filename_utf8;
}

int FileLoader::FindIndexInPathStack(const char *filename_utf8, const char* const* path_stack_utf8) const
{
	
	std::string fn;
	if(FileExists(filename_utf8))
		return -1;
	if(path_stack_utf8==nullptr||path_stack_utf8[0]==nullptr)
		return -2;
	int i=0;
	for(i=0;i<100;i++)
	{
		if(path_stack_utf8[i]==nullptr)
		{
			i--;
			break;
		}
	}
	double newest_date=0.0;
	
	int index=0;
	for(;i>=0;i--)
	{
		std::string f=std::string(path_stack_utf8[i]);
		/*std::vector<std::string> dir=ListDirectory(f);
		for(auto s:dir)
		{
			SIMUL_COUT<<s.c_str()<<std::endl;
		}*/
		if(f.length()>0&&f.back()!='/'&&f.back()!='\\')
			f+=std::string("/");
		f+=filename_utf8;
		if(FileExists(f.c_str()))
		{
			double filedate=GetFileDate(f.c_str());
			if(filedate>=newest_date)
			{
				fn=f;
				newest_date=filedate;
				index=i;
			}
		}
	}
	if(!FileExists(fn.c_str()))
		return -2;
	return index;
}

bool DefaultFileLoader::Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)
{
#if defined(__SWITCH__)
	std::string filepath = s_NVN_ROM_Name + std::string(filename_utf8);
	if (!FileExists(filename_utf8))
	{
		std::string path = filepath.substr(0, filepath.find_last_of("\\/") + 1);
		if (do_mkdir(path.c_str()) == 0)
		{
			nn::Result res = nn::fs::CreateFile(filepath.c_str(), static_cast<int64_t>(bytes));
			if (res.IsFailure())
			{
				SIMUL_CERR << "Failed to create file: " << filename_utf8 << std::endl;
				return false;
			}
		}
	}
	bool result = true;
	nn::fs::FileHandle fileHandle;
	nn::Result res = nn::fs::OpenFile(&fileHandle, filepath.c_str(), nn::fs::OpenMode::OpenMode_Write | nn::fs::OpenMode::OpenMode_AllowAppend);
	if (res.IsSuccess())
	{
		res = nn::fs::WriteFile(fileHandle, 0, pointer, bytes, nn::fs::WriteOption::MakeValue(nn::fs::WriteOptionFlag_Flush));
		if (res.IsFailure())
		{
			SIMUL_CERR << "Failed to write file: " << filename_utf8 << std::endl;
			result = false;
		}
	}
	nn::fs::CloseFile(fileHandle);
	return result;
#endif

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

#if defined(__SWITCH__)
void DefaultFileLoader::Set_NVN_ROM_Name(const std::string& name)
{
	s_NVN_ROM_Name = name;
	if (s_NVN_ROM_Name.find(":/") == std::string::npos)
		s_NVN_ROM_Name + ":/";
}
#endif

void FileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8, const std::vector<std::string> &paths, bool open_as_text)
{
	for(auto p: paths)
	{
		std::string str = p;
		str+="/";
		str += filename_utf8;
		if (FileExists(str.c_str()))
		{
			AcquireFileContents(pointer,bytes,str.c_str(),open_as_text);
			return;
		}
	}
}

void DefaultFileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text)
{
	if(!FileExists(filename_utf8))
	{
		SIMUL_CERR<<"Failed to find file "<<filename_utf8<<std::endl;
		pointer=NULL;
		return;
	}

#if __SWITCH__
	pointer = nullptr;
	bytes = 0;
	std::string filepath = s_NVN_ROM_Name + std::string(filename_utf8);
	nn::fs::FileHandle fileHandle;
	nn::Result res = nn::fs::OpenFile(&fileHandle, filepath.c_str(), nn::fs::OpenMode::OpenMode_Read);
	if (res.IsSuccess())
	{
		int64_t fileSize = 0;
		res = nn::fs::GetFileSize(&fileSize, fileHandle);
		if (res.IsFailure())
		{
			SIMUL_CERR << "Failed to get file size: " << filename_utf8 << std::endl;
		}
		else
		{
			pointer = malloc(fileSize + 1);
			bytes = static_cast<unsigned int>(fileSize);

			res = nn::fs::ReadFile(fileHandle, 0, pointer, fileSize);
			if (res.IsFailure())
				SIMUL_CERR << "Failed to read file: " << filename_utf8 << std::endl;
		}
	}
	nn::fs::CloseFile(fileHandle);
	return;
#endif
	
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
}

#ifdef _MSC_VER
static double GetDayNumberFromDateTime(int year,int month,int day,int hour,int min,int sec)
{
    int D = 367*year - (7*(year + ((month+9)/12)))/4 + (275*month)/9 + day - 730531;//was +2451545
	double d=(double)D;
	d+=(double)hour/24.0;
	d+=(double)min/24.0/60.0;
	d+=(double)sec/24.0/3600.0;
	return d;
}
#endif
double DefaultFileLoader::GetFileDate(const char* filename_utf8) const
{
	if(!FileExists(filename_utf8))
		return 0.0;

#if __SWITCH__
	std::string filepath = s_NVN_ROM_Name + std::string(filename_utf8);
	nn::fs::FileTimeStamp timeStamp = {};
	nn::Result res = nn::fs::GetFileTimeStampForDebug(&timeStamp, filepath.c_str());
	if (res.IsFailure())
		return 0.0;
	else
		return static_cast<double>(timeStamp.modify.value); //POSIX time!
	
#endif

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
    return (double)(fs::last_write_time(filename_utf8).time_since_epoch().count())/(3600.0*24.0*1000000.0);
#else
	return 0;
#endif
}

void DefaultFileLoader::ReleaseFileContents(void* pointer)
{
	free(pointer);
}

static DefaultFileLoader fl;
static FileLoader *fileLoader=&fl;

FileLoader *FileLoader::GetFileLoader()
{
	return fileLoader;
}

void FileLoader::SetFileLoader(FileLoader *f)
{
	fileLoader=f;
}

#if defined(_MSC_VER) && defined(_WIN32) && !defined(_GAMING_XBOX)
#include <filesystem>
std::string FileLoader::FindParentFolder(const char *folder_utf8)
{
#ifdef __cpp_lib_filesystem //Test for C++ 17 and filesystem. From https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
	using namespace std;
#else
	using namespace std::experimental;
#endif
	std::error_code ec;
#ifdef __cpp_lib_filesystem
	std::filesystem::path path = std::filesystem::current_path(ec);
#else
	std::experimental::filesystem::path path = std::experimental::filesystem::current_path(ec);
#endif
	std::wstring wspath(path.stem().c_str());
	std::string utf8path=WStringToUtf8(wspath);
	//std::string utf8path(wspath.begin(), wspath.end());
	while (utf8path.compare(folder_utf8) != 0)
	{
		if (!path.has_parent_path())
			return "";
		path = path.parent_path();
		wspath=path.stem().c_str();
		utf8path=WStringToUtf8(wspath);
	}
	std::wstring ws(path.c_str());
	std::string utf8=WStringToUtf8(ws);
	return utf8;
}
#else
std::string FileLoader::FindParentFolder(const char *)
{
	std::string utf8;
	return utf8;
}
#endif