#ifndef STRINGFUNCTIONS_H
#define STRINGFUNCTIONS_H

#include <string>
#include <set>
#include <stdarg.h>
#include "SfxClasses.h"


extern std::vector<std::string> SplitPath(const std::string& fullPath);
extern std::string NextName(const std::string &Line);
extern std::string GetFilenameOnly(const std::string &str);
extern std::string GetDirectoryFromFilename(const std::string &str);
extern std::string NextName(const std::string &Line,size_t &idx);
extern std::string ExtractNextName(std::string &Line);
extern void ClipWhitespace(std::string &Line);
extern int NextInt(const std::string &Line,size_t &idx);
extern int NextInt(const std::string &Line);
extern bool NextBool(const std::string &Line);
extern const float *NextVector3(const std::string &Line);
extern const float *NextVector2(const std::string &Line);
extern float NextFloat(const std::string &Line,size_t &idx);
extern float NextFloat(const std::string &Line);
extern float NextSpeed(const std::string &Line,size_t &idx);
extern float NextTorque(const std::string &Line);
extern float AsPhysicalValue(const std::string &Line);
extern float NextPower(const std::string &Line);
extern float NextTorque(const std::string &Line,size_t &idx);
extern float NextPower(const std::string &Line,size_t &idx);
extern size_t GoToLine(const std::string &Str,size_t line);
extern std::string ToString(int i);
extern std::string ToString(float i);
extern std::string ToString(bool i);
//! Create a std::string using sprintf-style formatting, with variable arguments.
extern std::string stringFormat(std::string fmt, ...);
/// A quick-and-dirty, non-re-entrant formatting function. Use this only for debugging.
extern const char *QuickFormat(const char *format_str,...);
//! Proper find-and-replace function for strings:
extern void find_and_replace(std::string& source, std::string const& find, std::string const& replace);
extern void find_and_replace_identifier(std::string& source, std::string const& find, std::string const& replace);

std::vector<std::string> split(const std::string &s, char delim);
std::string ToString(sfx::FillMode x);
std::string ToString(sfx::CullMode x);
std::string ToString(sfx::FilterMode x);
std::string ToString(sfx::AddressMode x);
extern std::string GetEnv(const std::string &name);
extern std::string join(const std::set<std::string> &replacements,std::string sep);
extern std::string join_vector(const std::vector<std::string> &replacements,std::string sep);
extern size_t count_lines_in_string(const std::string &str);
#endif

