#include "Platform/Core/DynamicLibrary.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/RuntimeError.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

using namespace platform::core;

DynamicLibrary::DynamicLibrary(const char* libraryFilepath)
{
#if defined(_WIN64)
	m_Handle = LoadLibraryA(libraryFilepath);
	if (!m_Handle)
	{
		SIMUL_ASSERT_WARN(false, platform::core::QuickFormat("%s does not exist. GetLastError: %ui", libraryFilepath, GetLastError()))
	}
#elif defined(__linux__)
	m_Handle = dlopen(libraryFilepath, RTLD_NOW | RTLD_NOLOAD);
	if (!m_Handle)
	{
		SIMUL_ASSERT_WARN(false, platform::core::QuickFormat("%s does not exist. dlerror: %s", libraryFilepath, dlerror()))
	}
#endif
}

DynamicLibrary::DynamicLibrary(Handle handle)
{
	m_Handle = handle;
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& library) noexcept
{
	m_Handle = library.m_Handle;
	library.m_Handle = 0;
}

DynamicLibrary::~DynamicLibrary()
{
	bool success = false;

#if defined(_WIN64)
	success = FreeLibrary((HMODULE)m_Handle);
	if (!success)
	{
		SIMUL_ASSERT_WARN(false, platform::core::QuickFormat("0x%x is not valid. GetLastError: %s", m_Handle, GetLastError()))
	}
#elif defined(__linux__)
	success = (dlclose(m_Handle) == 0);
	if (!success)
	{
		SIMUL_ASSERT_WARN(false, platform::core::QuickFormat("0x%x is not valid. dlerror: %s", m_Handle, dlerror()))
	}
#endif
}