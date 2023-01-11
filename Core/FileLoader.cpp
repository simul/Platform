#include "Platform/Core/FileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>
#if PLATFORM_STD_FILESYSTEM==1
#include <filesystem>
namespace fs = std::filesystem;
#elif PLATFORM_STD_FILESYSTEM==2
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
using namespace platform;
using namespace core;

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

std::string platform::core::GetExeDirectory()
{
#ifdef _WIN32
    // Windows specific
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW( NULL, szPath, MAX_PATH );
#else
    char szPath[PATH_MAX];
#if UNIX
    // Linux specific
    ssize_t count = readlink( "/proc/self/exe", szPath, PATH_MAX );
    if( count < 0 || count >= PATH_MAX )
        return {}; // some error
    szPath[count] = '\0';
#endif
#endif
	std::filesystem::path p=std::filesystem::path{ szPath }.parent_path() / ""; // to finish the folder path with (back)slash
	return p.string();
}

std::vector<std::string> FileLoader::ListDirectory(const std::string& path) const
{
	std::vector<std::string> dir;
#if PLATFORM_STD_FILESYSTEM
	try
	{
		if (!std::filesystem::exists(path))
		{
			SIMUL_COUT << "path does not exist: " << path.c_str() << std::endl;
			return dir;
		}
		if (!fs::is_directory(path))
		{
			SIMUL_COUT << "path is not directory: " << path.c_str() << std::endl;
			return dir;
		}
		for (const auto& entry : fs::directory_iterator(path))
			dir.push_back(entry.path().string());
	}
	catch (...)
	{
		SIMUL_COUT << "ListDirectory failed for path " << path.c_str() << std::endl;
	}
#else
#endif
	return dir;
}

std::string FileLoader::FindFileInPathStack(const char* filename_utf8, const std::vector<std::string>& path_stack_utf8) const
{
	const char** paths = new const char* [path_stack_utf8.size() + 1];
	for (size_t i = 0; i < path_stack_utf8.size(); i++)
	{
		paths[i] = path_stack_utf8[i].c_str();
	}
	paths[path_stack_utf8.size()] = nullptr;

	std::string f = FindFileInPathStack(filename_utf8, paths);
	delete[] paths;
	return f;
}

int FileLoader::FindIndexInPathStack(const char* filename_utf8, const std::vector<std::string>& path_stack_utf8) const
{
	const char** paths = new const char* [path_stack_utf8.size() + 1];
	for (size_t i = 0; i < path_stack_utf8.size(); i++)
	{
		paths[i] = path_stack_utf8[i].c_str();
	}
	paths[path_stack_utf8.size()] = nullptr;
	int i = FindIndexInPathStack(filename_utf8, paths);
	delete[] paths;
	return i;
}

std::string FileLoader::FindFileInPathStack(const char* filename_utf8, const char* const* path_stack_utf8) const
{
	int s = FindIndexInPathStack(filename_utf8, path_stack_utf8);
	if (s == -1)
		return filename_utf8;
	if (s < -1)
		return "";
	return std::string(path_stack_utf8[s]) + std::string("/") + filename_utf8;
}

int FileLoader::FindIndexInPathStack(const char* filename_utf8, const char* const* path_stack_utf8) const
{
	std::string fn;
	if (FileExists(filename_utf8))
		return -1;
	if (path_stack_utf8 == nullptr || path_stack_utf8[0] == nullptr)
		return -2;
	int i = 0;
	for (i = 0; i < 100; i++)
	{
		if (path_stack_utf8[i] == nullptr)
		{
			i--;
			break;
		}
	}
	double newest_date = 0.0;

	int index = 0;
	for (; i >= 0; i--)
	{
		std::string f = std::string(path_stack_utf8[i]);
		//std::vector<std::string> dir=ListDirectory(f);
	/*	for(auto s:dir)
		{
			std::cout<<s.c_str()<<std::endl;
		}*/
		if (f.length() > 0 && f.back() != '/' && f.back() != '\\')
			f += std::string("/");
		f += filename_utf8;
		if (FileExists(f.c_str()))
		{
			double filedate = GetFileDate(f.c_str());
			if (filedate >= newest_date)
			{
				fn = f;
				newest_date = filedate;
				index = i;
			}
		}
	}
	if (!FileExists(fn.c_str()))
		return -2;
	return index;
}

std::string FileLoader::LoadAsString(const char* filename_utf8)
{
	static std::vector<std::string> paths={""};
	return LoadAsString(filename_utf8,paths);
}

std::string FileLoader::LoadAsString(const char* filename_utf8, const std::vector<std::string>& paths)
{
	std::string str;
	void *ptr=nullptr;
	unsigned bytelen=0;
	AcquireFileContents(ptr,bytelen,filename_utf8,paths,true);
	if(ptr)
		str=(char*)ptr;
	ReleaseFileContents(ptr);
	return str;
}

void FileLoader::AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8, const std::vector<std::string>& paths, bool open_as_text)
{
	for (auto p : paths)
	{
		std::string str = p;
		if(p.length())
			str += "/";
		str += filename_utf8;
		if (FileExists(str.c_str()))
		{
			AcquireFileContents(pointer, bytes, str.c_str(), open_as_text);
			return;
		}
	}
}

#if defined (_MSC_VER) && !defined(_GAMING_XBOX)
#include <filesystem>
std::string FileLoader::FindParentFolder(const char* folder_utf8)
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
	std::string utf8path = core::WStringToUtf8(wspath);
	//std::string utf8path(wspath.begin(), wspath.end());
	while (utf8path.compare(folder_utf8) != 0)
	{
		if (!path.has_parent_path())
			return "";
		path = path.parent_path();
		wspath = path.stem().c_str();
		utf8path = WStringToUtf8(wspath);
	}
	std::wstring ws(path.c_str());
	std::string utf8 = WStringToUtf8(ws);
	return utf8;
}
#else
std::string FileLoader::FindParentFolder(const char*)
{
	std::string utf8;
	return utf8;
}
#endif