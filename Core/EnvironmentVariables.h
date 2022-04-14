#ifndef SIMUL_BASE_ENVIRONMENTVARIABLES_H
#define SIMUL_BASE_ENVIRONMENTVARIABLES_H
#include <string>
#include "Platform/Core/Export.h"
namespace platform
{
	namespace core
	{
		extern PLATFORM_CORE_EXPORT void SetUseExternalTextures(bool t);
		/// See SetUseExternalTextures.
		extern PLATFORM_CORE_EXPORT bool GetUseExternalTextures();
		/// A class to manage environment variables.
		PLATFORM_CORE_EXPORT_CLASS EnvironmentVariables
		{
		public:
		//! Get the named environment variable (Windows only, other platforms return an empty string.
			static std::string GetSimulEnvironmentVariable(const std::string &name);
		//! Set the named environment variable for this process.
			static std::string SetSimulEnvironmentVariable(const std::string &name,const std::string &value);
			static std::string AppendSimulEnvironmentVariable(const std::string &name,const std::string &value);
		//! Get the current directory.
			static std::string GetWorkingDirectory();
		//! Get the directory where the current exe is held.
			static std::string GetExecutableDirectory();
		//! Set the current working directory.
			static void SetWorkingDirectory(std::string);
		};
	}
}
#endif
