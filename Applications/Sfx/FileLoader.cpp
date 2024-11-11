#include "FileLoader.h"
#include <iostream>
#ifdef _MSC_VER
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h> // for fopen, seek, fclose
#include <stdlib.h> // for malloc, free
#include <time.h>
#include "StringFunctions.h"
#include "StringToWString.h"
typedef struct stat Stat;

static int do_mkdir(const char *path_utf8)
{
	int			 status = 0;
#ifdef _MSC_VER
	struct _stat64i32			st;
	std::wstring wstr=Utf8ToWString(path_utf8);
	if (_wstat (wstr.c_str(), &st) != 0)
#else
	Stat			st;
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
int mkpath(const std::string &filename_utf8)
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

FileLoader::FileLoader()
{
}
#ifdef _MSC_VER
#pragma optimize("",off)
#endif
bool FileLoader::FileExists(const char *filename_utf8) const
{
	
	enum access_mode
	{
		NO_FILE=-1,EXIST=0,WRITE=2,READ=4
	};
#ifdef _MSC_VER
	std::wstring wstr=Utf8ToWString(filename_utf8);
	access_mode res=(access_mode)_waccess(wstr.c_str(),READ);
	bool bExists = (res!=NO_FILE);
	if(errno==EACCES)
	{
		std::cerr<<"FileLoader::FileExists Access denied: for "<<filename_utf8<<" the file's permission setting does not allow specified access."<<std::endl;
	}
	else if(errno==EINVAL)
	{
		std::cerr<<"FileLoader::FileExists Invalid parameter: for "<<filename_utf8<<".\n";
	}
	else if(errno==ENOENT)
	{
		// ENOENT: File name or path not found.
		// This is an acceptable error as we are just checking whether the file exists.
	}
	// If res is NO_FILE, errno will now be set, so we must clear it.
	errno=0;
#else
	Stat st;
	bool bExists=(stat(filename_utf8, &st)==0);
	errno=0;
#endif
	return bExists;
}

const char *FileLoader::FindFileInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const
{
	static std::string fn;
	if(FileExists(filename_utf8))
	{
		char buffer[_MAX_PATH];
		fn=filename_utf8;
		#ifdef _MSC_VER
		if(fn.find(":")>=fn.length())
			if(_getcwd(buffer,_MAX_PATH))
		#else
		if(fn.find("/")!=0)
			if(getcwd(buffer,_MAX_PATH))
		#endif
		{
			fn=buffer;
			fn+="/";
			fn+=filename_utf8;
		}
		return fn.c_str();
	}
	for(int i=(int)path_stack_utf8.size()-1;i>=0;i--)
	{
		fn=(path_stack_utf8[i]+"/")+filename_utf8;
		if(FileExists(fn.c_str()))
			break;
	}
	if(!FileExists(fn.c_str()))
		return "";
	return fn.c_str();
}

void FileLoader::Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)
{
	
	std::wstring wstr=Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
	// Ensure the directory exists:
	{
		mkpath(filename_utf8);
	}
	
#ifdef _MSC_VER
	_wfopen_s(&fp,wstr.c_str(),L"wb");//w, ccs=UTF-8
#else
	fp = fopen(filename_utf8,"wb");//w, ccs=UTF-8
#endif
	
	if(!fp)
	{
		std::cerr<<"Failed to open file "<<filename_utf8<<std::endl;
		return;
	}
	fseek(fp, 0, SEEK_SET);
	fwrite(pointer, 1, bytes, fp);
	if(save_as_text)
	{
		char c=0;
		fwrite(&c, 1, 1, fp);
	}
	fclose(fp);
	
}

void FileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text)
{
	if(!FileExists(filename_utf8))
	{
		std::cerr<<"Failed to find file "<<filename_utf8<<std::endl;
		pointer=NULL;
		return;
	}
	
	std::wstring wstr=Utf8ToWString(filename_utf8);
	FILE *fp = NULL;
#ifdef _MSC_VER
	_wfopen_s(&fp,wstr.c_str(),L"rb");//open_as_text?L"r":L"rb")
#else
	fp = fopen(filename_utf8,"rb");//r, ccs=UTF-8
#endif
	
	fseek(fp, 0, SEEK_END);
	bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pointer = malloc(bytes+1);
	fread(pointer, 1, bytes, fp);
	if(open_as_text)
		((char*)pointer)[bytes]=0;
	
	fclose(fp);
}

uint64_t FileLoader::GetFileDate(const char *filename_utf8)
{
	std::wstring filenamew=StringToWString(filename_utf8);
	#ifdef _MSC_VER
	struct _stat64i32 buf;
	_wstat(filenamew.c_str(), &buf);
	#else
	struct stat buf;
	stat(filename_utf8, &buf);
	#endif
	buf.st_mtime;
	time_t t = buf.st_mtime;
	return static_cast<uint64_t>(t);
}		

void FileLoader::ReleaseFileContents(void* pointer)
{
	free(pointer);
}