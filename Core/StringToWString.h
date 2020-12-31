#ifndef SIMUL_BASE_STRINGTOWSTRING_H
#define SIMUL_BASE_STRINGTOWSTRING_H
#include <string>
#include "Platform/Core/Export.h"
namespace simul
{
	namespace base
	{
		extern PLATFORM_CORE_EXPORT std::wstring Utf8ToWString(const char *utf8);
		extern PLATFORM_CORE_EXPORT std::wstring Utf8ToWString(std::string utf8);
		extern PLATFORM_CORE_EXPORT std::string WStringToUtf8(const wchar_t *src_w);
		extern PLATFORM_CORE_EXPORT std::string WStringToUtf8(std::wstring wstr);
		extern PLATFORM_CORE_EXPORT std::wstring StringToWString(const std::string &text);
		extern PLATFORM_CORE_EXPORT std::string WStringToString(const std::wstring &text);
		extern PLATFORM_CORE_EXPORT void WStringToAsciiString(char *output,const std::wstring &text);
	}
}
#endif