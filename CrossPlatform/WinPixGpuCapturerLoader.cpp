#if PLATFORM_LOAD_WINPIXGPUCAPTURER
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/WinPixGpuCapturerLoader.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>

using namespace platform;
using namespace crossplatform;

static HMODULE s_HModuleWinPixGpuCapturer;
static std::filesystem::path s_WinPixGpuCapturerFullpath;

#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a
#define PLATFORM_SOURCE_DIR_STR STRINGIFY(PLATFORM_SOURCE_DIR)

void WinPixGpuCapturerLoader::Load()
{
	if (s_HModuleWinPixGpuCapturer)
		return;

	ERRNO_BREAK
	LPWSTR programFilesPath;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, 0, &programFilesPath);
	s_WinPixGpuCapturerFullpath = platform::core::WStringToString(programFilesPath);
	s_WinPixGpuCapturerFullpath /= "Microsoft PIX";
	std::filesystem::path pixVersionFolder;
	for (auto const& directory : std::filesystem::directory_iterator(s_WinPixGpuCapturerFullpath))
	{
		if (directory.is_directory())
		{
			if (pixVersionFolder.empty())
			{
				pixVersionFolder = directory.path();
				continue;
			}

			float dirPixVersion = std::stof(directory.path().lexically_relative(s_WinPixGpuCapturerFullpath));
			float curPixVersion = std::stof(pixVersionFolder.lexically_relative(s_WinPixGpuCapturerFullpath));
			if (dirPixVersion > curPixVersion)
			{
				pixVersionFolder = directory.path();
			}
		}
	}
	s_WinPixGpuCapturerFullpath /= pixVersionFolder / "WinPixGpuCapturer.dll";
	s_HModuleWinPixGpuCapturer = LoadLibraryA(s_WinPixGpuCapturerFullpath.generic_string().c_str());
	errno = 0;
	if (!s_HModuleWinPixGpuCapturer)
	{
		std::string error_str = "WinPixGpuCapturerLoader was unable to load '" + s_WinPixGpuCapturerFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
		SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		ERRNO_BREAK
			return;
	}

	ERRNO_BREAK
}

void WinPixGpuCapturerLoader::Unload()
{
	if (s_HModuleWinPixGpuCapturer)
	{
		if (!FreeLibrary(s_HModuleWinPixGpuCapturer))
		{
			std::string error_str = "WinPixGpuCapturerLoader was unable to free '" + s_WinPixGpuCapturerFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
			SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		}
		errno = 0;
	}
}
void WinPixGpuCapturerLoader::TriggerMultiFrameCapture(unsigned num) {}
void WinPixGpuCapturerLoader::StartCapture(RenderPlatform* renderPlatform, void* windowHandlePtr) {}
void WinPixGpuCapturerLoader::FinishCapture() {}
bool WinPixGpuCapturerLoader::IsLoaded()
{
	return (s_HModuleWinPixGpuCapturer ? true : false);
}

#else //Dummy class implementation
#include "Platform/CrossPlatform/WinPixGpuCapturerLoader.h"

using namespace platform;
using namespace crossplatform;

void WinPixGpuCapturerLoader::Load() {}
void WinPixGpuCapturerLoader::Unload() {}
void WinPixGpuCapturerLoader::StartCapture(RenderPlatform* renderPlatform, void* windowHandlePtr) {}
void WinPixGpuCapturerLoader::FinishCapture() {}
bool WinPixGpuCapturerLoader::IsLoaded() { return false; }
#endif