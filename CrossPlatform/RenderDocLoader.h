#pragma once

#include "Platform/CrossPlatform/Export.h"
#ifdef _WIN32

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include "Platform/External/RenderDoc/Include/renderdoc_app.h"

namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT RenderDocLoader
		{
		public:
			static void Load();
			static void Unload();
		
		private:
			static RENDERDOC_API_1_4_1* s_RenderDocAPI;

			static HMODULE s_HModuleRenderDoc;
			static std::filesystem::path s_RenderDocFullpath;
		};
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#else //Dummy class
namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT RenderDocLoader
		{
		public:
			static void Load();
			static void Unload();
		};
	}
}
#endif