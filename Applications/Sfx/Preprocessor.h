#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H
// shared #include file for Lexer and Parser.
#include <vector>
#include <string>
#include <map>
enum Test
{
	IS_DEFINED,NOT_DEFINED,ZERO,NONZERO,EQUALS,NOTEQUAL,GREATER,LESS,GREATER_EQUAL,LESS_EQUAL
};
struct PreprocessorType
{
	PreprocessorType() {}
	union
	{
		int num;
	};
	std::string str;
	std::vector<std::string> strs;
	Test test;
};
struct MacroDefinition
{
	std::vector<std::string> definition;
	std::vector<std::string> parameters;
	std::string filename;
	int line_number;
};
// prevent echoing of unprocessed chars:
#define ECHO
extern uint64_t latest_datetime;
extern std::string latest_file;
#undef PREPRO_STYPE
#define PREPRO_STYPE PreprocessorType
#undef PREPRO_DEBUG
#define PREPRO_DEBUG 1
// These are the callback functions for file handling that we will send to the preprocessor.
extern FILE* (*prepro_open)(const char *filename_utf8,std::string &fullPathName,uint64_t &datetime);
extern void (*prepro_close)(FILE *f);
extern void Unput(int c);
extern std::ostringstream preproOutput;
extern bool preprocess(const char *file, std::map<std::string, std::string> defines = std::map<std::string, std::string>(),bool=false);
// Are we in an active  #if #endif block?
extern bool inActiveBlock();
extern std::vector<std::string> GetPreprocessorFilenamesUtf8();
extern void Write(const char *);
extern void WriteLineNumber();
extern void WriteLineNumber(int);
extern bool IsDefined(std::string name);
extern void DefineMacro(std::string name,std::string params,const std::vector<std::string> &definition);
extern const MacroDefinition *GetMacro(std::string name);
extern void UndefineMacro(std::string name);
extern std::string ProcessMacro(std::string id,const std::vector<std::string> &params);
extern void ClearParserState();
extern void Skip();
#endif