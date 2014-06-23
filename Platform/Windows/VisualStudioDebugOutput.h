#pragma once
#include "Simul/Base/BufferedStringStreamBuf.h"
#include "Simul/Base/RuntimeError.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <direct.h>
#include <cerrno>

class VisualStudioDebugOutput : public simul::base::BufferedStringStreamBuf
{
public:
    VisualStudioDebugOutput(bool send_to_output_window=true,
							const char *logfilename=NULL,size_t bufsize=(size_t)16)
		:simul::base::BufferedStringStreamBuf((int)bufsize)
		,old_cout_buffer(NULL)
		,old_cerr_buffer(NULL)
		,to_logfile(false)
	{
	ERRNO_CHECK
		//if(errno!=0)
		//	simul::base::RuntimeError(strerror(errno));
		to_output_window=send_to_output_window;
		if(logfilename)
			setLogFile(logfilename);
		old_cout_buffer=std::cout.rdbuf(this);
		old_cerr_buffer=std::cerr.rdbuf(this);
	ERRNO_CHECK
	}
	virtual ~VisualStudioDebugOutput()
	{
		if(to_logfile)
		{
			logFile<<std::endl;
			logFile.close();
		}
		std::cout.rdbuf(old_cout_buffer);
		std::cerr.rdbuf(old_cerr_buffer);
	}
	void setLogFile(const char *logfilename)
	{
	ERRNO_CHECK
		std::string fn=logfilename;
		if(fn.find(":")>=fn.length())
		{
			char buffer[_MAX_PATH];
#ifndef _XBOX_ONE
			if(_getcwd(buffer,_MAX_PATH))
			{
				fn=buffer;
			}
#endif
			fn+="/";
			fn+=logfilename;
		}
		logFile.open(fn.c_str());
		if(errno!=0)
			simul::base::RuntimeError(strerror(errno));
		if(logFile.good())
			to_logfile=true;
	}
    virtual void writeString(const std::string &str)
    {
		if(to_logfile)
			logFile<<str.c_str()<<std::endl;
		if(to_output_window)
		{
#ifdef UNICODE
			std::wstring wstr(str.length(), L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(str.begin(), str.end(), wstr.begin());
			OutputDebugString(wstr.c_str());
#else
	        OutputDebugString(str.c_str());
#endif
		}

    }
protected:
	std::ofstream logFile;
	bool to_output_window;
	bool to_logfile;
	std::streambuf *old_cout_buffer;
	std::streambuf *old_cerr_buffer;
};