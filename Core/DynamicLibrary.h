#pragma once

#include "Platform/Core/Export.h"

namespace platform
{
	namespace core
	{
		class PLATFORM_CORE_EXPORT DynamicLibrary
		{
		public:
			typedef void* Handle;

		public:
			DynamicLibrary() = default;
			DynamicLibrary(const char* libraryFilepath);
			DynamicLibrary(Handle handle);
			DynamicLibrary(DynamicLibrary&) = delete;
			DynamicLibrary(DynamicLibrary&& library) noexcept;
			~DynamicLibrary();

			operator const Handle&() const { return m_Handle; }
			operator Handle() { return m_Handle; }
			operator bool() { return m_Handle != nullptr; }

		private:
			Handle m_Handle = nullptr;
		};
	}
}