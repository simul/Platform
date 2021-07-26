#ifndef SIMUL_BASE_STRINGFUNCTIONS_H
#define SIMUL_BASE_STRINGFUNCTIONS_H

#include <string>
#include <vector>
#include <stdarg.h>
#ifdef UNIX
#include <strings.h> // for strcasecmp
#endif
#include "Platform/Core/Export.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#define _stricmp strcasecmp
#endif

// Implement snprintf for MS compilers
 #if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf
 
inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}

#endif

namespace simul
{
	namespace base
	{

		extern PLATFORM_CORE_EXPORT std::vector<std::string> SplitPath(const std::string& fullPath);
		extern PLATFORM_CORE_EXPORT bool GetDelimited(const std::string &str,std::string &First,std::string &Remainder,char delim);
		extern PLATFORM_CORE_EXPORT std::string ExtractDelimited(std::string &str,char delim);
		extern PLATFORM_CORE_EXPORT std::string ExtractDelimited(std::string &str,const std::string &delim);
		extern PLATFORM_CORE_EXPORT std::string ExtractDelimited(std::string &str,char delim,bool match_brackets);
		extern PLATFORM_CORE_EXPORT std::string ExtractDelimited(std::string &str,const std::string &delim,bool match_brackets);
		extern PLATFORM_CORE_EXPORT bool GetDelimited(const std::string &str,std::string &First,std::string &Remainder,const std::string &delim);
		   
		extern PLATFORM_CORE_EXPORT bool NextSettingFromTag(std::string &Line,std::string &Name,std::string &Value);
		extern PLATFORM_CORE_EXPORT void StartXMLWrite(std::string &out);
		extern PLATFORM_CORE_EXPORT std::string NextName(const std::string &Line);
		extern PLATFORM_CORE_EXPORT void StripDirectoryFromFilename(std::string &str);
		extern PLATFORM_CORE_EXPORT std::string GetDirectoryFromFilename(const std::string &str);
		extern PLATFORM_CORE_EXPORT std::string NextName(const std::string &Line,size_t &idx);
		extern PLATFORM_CORE_EXPORT std::string ExtractNextName(std::string &Line);
		extern PLATFORM_CORE_EXPORT void ClipWhitespace(std::string &Line);
		extern PLATFORM_CORE_EXPORT int NextInt(const std::string &Line,size_t &idx);
		extern PLATFORM_CORE_EXPORT int NextInt(const std::string &Line);
		extern PLATFORM_CORE_EXPORT bool NextBool(const std::string &Line);
		extern PLATFORM_CORE_EXPORT const float *NextVector3(const std::string &Line);
		extern PLATFORM_CORE_EXPORT const float *NextVector2(const std::string &Line);
		extern PLATFORM_CORE_EXPORT float NextFloat(const std::string &Line,size_t &idx);
		extern PLATFORM_CORE_EXPORT float NextFloat(const std::string &Line);
		extern PLATFORM_CORE_EXPORT float AsPhysicalValue(const std::string &Line);
		extern PLATFORM_CORE_EXPORT size_t GoToLine(const std::string &Str,size_t line);
		extern PLATFORM_CORE_EXPORT std::string StringOf(int i);
		extern PLATFORM_CORE_EXPORT std::string StringOf(float i);
		extern PLATFORM_CORE_EXPORT std::string stringof(int i);
		extern PLATFORM_CORE_EXPORT std::string stringof(float f);
		//! Create a std::string using sprintf-style formatting, with variable arguments.
		extern PLATFORM_CORE_EXPORT std::string stringFormat(std::string fmt, ...);
		/// A quick-and-dirty, non-re-entrant formatting function. Use this only for debugging.
		extern PLATFORM_CORE_EXPORT const char *QuickFormat(const char *format_str,...);
		//! Proper find-and-replace function for strings:
		extern PLATFORM_CORE_EXPORT void find_and_replace(std::string& source, std::string const& find, std::string const& replace);
		//! Divide a string into a vector of smaller strings, based on the given separator
		extern PLATFORM_CORE_EXPORT std::vector<std::string> split(const std::string& source, char separator);
		extern PLATFORM_CORE_EXPORT std::string toNext(const std::string& source,char separator,size_t &pos);
	}
}
#endif

