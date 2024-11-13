#include "Platform/Core/FileLoader.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>

#if PLATFORM_STD_FILESYSTEM == 1
#include <filesystem>
namespace fs = std::filesystem;
#elif PLATFORM_STD_FILESYSTEM == 2
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

#ifdef UNIX
#include <linux/limits.h>
#endif

std::string platform::core::GetExeDirectory()
{
#ifdef _WIN32
	// Windows specific
	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW( NULL, szPath, MAX_PATH );
#endif

#ifdef UNIX
	// Linux specific
	char szPath[PATH_MAX];
	ssize_t count = readlink( "/proc/self/exe", szPath, PATH_MAX );
	if( count < 0 || count >= PATH_MAX )
		return {}; // some error
	szPath[count] = '\0';
#endif

	//TODO! Need a better solution. -AJR
	//This is based on the Local Path in the Visual Studio Project settings,
	//where the PS5 Standard Debugger maps the Local Path to /app0/.
	//CMake sets the Local Path (VS_DEBUGGER_WORKING_DIRECTORY) to ${CMAKE_SOURCE_DIR}.
#ifdef __COMMODORE__
	fs::path cmake_binary_dir = PLATFORM_STRING_OF_MACRO(CMAKE_BINARY_DIR);
	fs::path cmake_source_dir = PLATFORM_STRING_OF_MACRO(CMAKE_SOURCE_DIR);
	fs::path relative_path = cmake_binary_dir.lexically_relative(cmake_source_dir);
	#ifdef DEBUG
		return "/app0/" + relative_path.generic_string() + "/bin/Debug/";
	#else
		return "/app0/" + relative_path.generic_string() + "/bin/Release/";
	#endif
#endif

#if PLATFORM_STD_FILESYSTEM > 0 && (defined(_WIN32) || defined(UNIX))
	fs::path p = fs::path{szPath}.parent_path() / ""; // to finish the folder path with (back)slash
	return p.string();
#else
	return std::string();
#endif
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
	fileLoader = f;
}

int FileLoader::NextSlash(const std::string &str, int pos)
{
	int slash = (int)str.find('/', pos);
	int back = (int)str.find('\\', pos);
	if (slash < 0 || (back >= 0 && back < slash))
		slash = back;
	return slash;
}

#if defined(_MSC_VER) && !defined(_GAMING_XBOX) && !defined(__SWITCH__) && PLATFORM_STD_FILESYSTEM > 0
std::string FileLoader::FindParentFolder(const char *folder_utf8)
{
	std::error_code ec;
	fs::path path = fs::current_path(ec);
	std::wstring wspath(path.stem().c_str());
	std::string utf8path = core::WStringToUtf8(wspath);
	// std::string utf8path(wspath.begin(), wspath.end());
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
std::string FileLoader::FindParentFolder(const char *)
{
	std::string utf8;
	return utf8;
}
#endif

std::string FileLoader::LoadAsString(const char *filename_utf8)
{
	static std::vector<std::string> paths = {""};
	return LoadAsString(filename_utf8, paths);
}

std::string FileLoader::LoadAsString(const char *filename_utf8, const std::vector<std::string> &paths)
{
	std::string str;
	void *ptr = nullptr;
	unsigned bytelen = 0;
	AcquireFileContents(ptr, bytelen, filename_utf8, paths, true);
	if (ptr)
		str = (char *)ptr;
	ReleaseFileContents(ptr);
	return str;
}

void FileLoader::AcquireFileContents(void *&pointer, unsigned int &bytes, const char *filename_utf8, const std::vector<std::string> &paths, bool open_as_text)
{
	for (auto p : paths)
	{
		std::string str = p;
		if (p.length())
			str += "/";
		str += filename_utf8;
		if (FileExists(str.c_str()))
		{
			AcquireFileContents(pointer, bytes, str.c_str(), open_as_text);
			return;
		}
	}
}

std::vector<std::string> FileLoader::ListDirectory(const std::string& path) const
{
	std::vector<std::string> dir;
#if PLATFORM_STD_FILESYSTEM
	try
	{
		if (!fs::exists(path))
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
	std::string abs_path = std::string(filename_utf8);
	if(abs_path.find(":")!=std::string::npos)
	{
		if (FileExists(abs_path.c_str())) 
		// absolute path
			return -1;
		else
			return -2;
	}
	int i = 0;
	for (i = 0; i < 100; i++)
	{
		if (path_stack_utf8[i] == nullptr)
		{
			i--;
			break;
		}
	}

	uint64_t newest_date = 0;
	int index = 0;
	for (; i >= 0; i--)
	{
		std::string f = std::string(path_stack_utf8[i]);
		//std::vector<std::string> dir=ListDirectory(f);
		/*for(auto s:dir)
		{
			std::cout<<s.c_str()<<std::endl;
		}*/
		if (f.length() > 0 && f.back() != '/' && f.back() != '\\')
			f += std::string("/");
		f += filename_utf8;
		if (FileExists(f.c_str()))
		{
			uint64_t filedate = GetFileDate(f.c_str());
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
