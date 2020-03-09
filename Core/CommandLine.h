#pragma once
#include <functional>
#include <string>
#include "Platform/Core/Export.h"
namespace simul
{
	namespace base
	{
		typedef std::function<void(const std::string &)> OutputDelegate;
		extern PLATFORM_CORE_EXPORT bool RunCommandLine(const char *command_utf8,  OutputDelegate outputDelegate=OutputDelegate());
	}
}
