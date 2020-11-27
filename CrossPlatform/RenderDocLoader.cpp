#if PLATFORM_LOAD_RENDERDOC
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/CrossPlatform/RenderDocLoader.h"

using namespace simul;
using namespace crossplatform;

RENDERDOC_API_1_4_1* RenderDocLoader::s_RenderDocAPI = nullptr;
HMODULE RenderDocLoader::s_HModuleRenderDoc;
std::filesystem::path RenderDocLoader::s_RenderDocFullpath;

#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a
#define PLATFORM_SOURCE_DIR_STR STRINGIFY(PLATFORM_SOURCE_DIR)

void RenderDocLoader::Load() 
{
	if (s_HModuleRenderDoc)
		return;

	std::string platform_dir = PLATFORM_SOURCE_DIR_STR;
	s_RenderDocFullpath = platform_dir + "/External/RenderDoc/lib/x64/renderdoc.dll";
	s_HModuleRenderDoc = LoadLibraryA(s_RenderDocFullpath.generic_string().c_str());
	errno=0;
	if (!s_HModuleRenderDoc)
	{
		std::string error_str = "RenderDocLoader was unable to load '" + s_RenderDocFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
		SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		return;
	}

	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(s_HModuleRenderDoc, "RENDERDOC_GetAPI");
	if (!RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1, (void**)&s_RenderDocAPI))
	{
		SIMUL_CERR_ONCE << "RenderDocLoader was unable to initialise RenderDoc.\n";
		return;
	}
}

void RenderDocLoader::Unload() 
{
	if (s_HModuleRenderDoc)
	{
		if (!FreeLibrary(s_HModuleRenderDoc))
		{
			std::string error_str = "RenderDocLoader was unable to free '" + s_RenderDocFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
			SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		}
		errno = 0;
	}
}
#else //Dummy class implementation
#include "Platform/CrossPlatform/RenderDocLoader.h"

using namespace simul;
using namespace crossplatform;

void RenderDocLoader::Load() {}
void RenderDocLoader::Unload() {}
#endif