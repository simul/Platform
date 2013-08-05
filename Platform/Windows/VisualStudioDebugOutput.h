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
							bool send_to_logfile=true,
							const char *logfilename=NULL,size_t bufsize=(size_t)16)
		:simul::base::BufferedStringStreamBuf((int)bufsize)
		,old_cout_buffer(NULL)
		,old_cerr_buffer(NULL)
	{
		if(errno!=0)
			simul::base::RuntimeError(strerror(errno));
		to_output_window=send_to_output_window;
		to_logfile=send_to_logfile;
		char buffer[_MAX_PATH];
		if(_getcwd(buffer,_MAX_PATH))
		{
			
		}
		std::string fn=buffer;
		if(send_to_logfile)
		{
			if(!logfilename)
				logfilename="logfile.txt";
			fn+="/";
			fn+=logfilename;
			logFile.open(fn.c_str());
		if(errno!=0)
			simul::base::RuntimeError(strerror(errno));
		}
		old_cout_buffer=std::cout.rdbuf(this);
		old_cerr_buffer=std::cerr.rdbuf(this);
	}
	virtual ~VisualStudioDebugOutput()
	{
		logFile<<std::endl;
		logFile.close();
		std::cout.rdbuf(old_cout_buffer);
		std::cerr.rdbuf(old_cerr_buffer);
	}
    virtual void writeString(const std::string &str)
    {
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