#pragma once
#include "Simul/Base/BufferedStringStreamBuf.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>

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
		to_output_window=send_to_output_window;
		to_logfile=send_to_logfile;
		if(send_to_logfile)
		{
			if(logfilename)
				logFile.open(logfilename);
			else
				logFile.open("logfile.txt");
		}
		old_cout_buffer=std::cout.rdbuf(this);
		old_cerr_buffer=std::cerr.rdbuf(this);
		
		time_t rawtime;
		rawtime = time (&rawtime);
		struct tm timeinfo;
		localtime_s(&timeinfo,&rawtime);
		std::cout <<asctime (&timeinfo) <<std::endl;
		std::cout << "----------------"<<std::endl;
		std::cout << "Begin Log output"<<std::endl;
		std::cout << "----------------"<<std::endl<<std::endl;
	}
	virtual ~VisualStudioDebugOutput()
	{
		std::cout.rdbuf(old_cout_buffer);
		std::cerr.rdbuf(old_cerr_buffer);
	}
    virtual void writeString(const std::string &str)
    {
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
		if(to_logfile)
		{
			logFile<<str.c_str();
		}
    }
protected:
	std::ofstream logFile;
	bool to_output_window;
	bool to_logfile;
	std::streambuf *old_cout_buffer;
	std::streambuf *old_cerr_buffer;
};