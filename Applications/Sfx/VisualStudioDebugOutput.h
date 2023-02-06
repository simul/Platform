#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include "BufferedStringStreamBuf.h"
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define __stdcall
bool IsDebuggerPresent() {return false;}
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <string.h>// for strerror_s
#include <cerrno>
#include <cstring>
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

typedef void (__stdcall *DebugOutputCallback)(const char *);

class VisualStudioDebugOutput : public BufferedStringStreamBuf
{
public:
	VisualStudioDebugOutput(bool send_to_output_window=true,
							const char *logfilename=NULL,size_t bufsize=(size_t)16
							,DebugOutputCallback c=NULL)
		:BufferedStringStreamBuf((int)bufsize)
		,to_logfile(false)
		,old_cout_buffer(NULL)
		,old_cerr_buffer(NULL)
		,callback(c)
	{
		if(IsDebuggerPresent())
		{
			to_output_window=send_to_output_window;
			old_cout_buffer = std::cout.rdbuf(this);
			old_cerr_buffer = std::cerr.rdbuf(this);
			FILE *fstream;
			freopen_s(&fstream,"trace.txt", "w", stderr);
		}
		if(logfilename)
			setLogFile(logfilename);
	}
	virtual ~VisualStudioDebugOutput()
	{
		//con_out.close();
		//con_err.close();
		if(IsDebuggerPresent())
		{
			if(to_logfile)
			{
				logFile<<std::endl;
				logFile.close();
			}
			std::cout.rdbuf(old_cout_buffer);
			std::cerr.rdbuf(old_cerr_buffer);
		}
	}
	void setLogFile(const char *logfilename)
	{
		std::string fn=logfilename;
		if(fn.find(":")>=fn.length())
		{
			char buffer[_MAX_PATH];
#ifdef _MSC_VER
			if(_getcwd(buffer,_MAX_PATH))
#else
			if(getcwd(buffer,_MAX_PATH))
#endif
			{
				fn=buffer;
			}
			fn+="/";
			fn+=logfilename;
		}
		errno=0;
		logFile.open(fn.c_str());
		if(errno!=0)
			writeString("Failed to open logfile.");
		if(logFile.good())
			to_logfile=true;
	}
	void setCallback(DebugOutputCallback c)
	{
		callback=c;
	}
	virtual void writeString(const std::string &str)
	{
		fprintf(stdout,"%s",str.c_str());
		//con_out<<str.c_str();
		if(to_logfile)
			logFile<<str.c_str()<<std::endl;
		if(callback)
		{
			callback(str.c_str());
		}
		if(to_output_window)
		{
#ifdef _MSC_VER
#ifdef UNICODE
			std::wstring wstr(str.length(), L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(str.begin(), str.end(), wstr.begin());
			OutputDebugString(wstr.c_str());
#else
			OutputDebugString(str.c_str());
#endif
#else
			printf("%s",str.c_str());
#endif
		}
	}
protected:
	std::ofstream logFile;

	bool to_output_window;
	bool to_logfile;
	std::streambuf *old_cout_buffer;
	std::streambuf *old_cerr_buffer;
	DebugOutputCallback callback;
};

#undef _CRT_SECURE_NO_WARNINGS