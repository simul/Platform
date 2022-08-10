#pragma once
#include <vector>
#include <string>
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifdef UNIX
#define _getcwd getcwd
#endif

//! Class for loading files
class FileLoader
{
public:
	FileLoader();
	bool FileExists(const char *filename_utf8) const;
	const char *FindFileInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const;
	void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text);
	double GetFileDate(const char* filename_utf8);
	void ReleaseFileContents(void* pointer);
	void Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text);
};

extern int mkpath(const std::string &filename_utf8);