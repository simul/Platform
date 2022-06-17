#pragma once
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#else
#define OutputDebugString
#define _getcwd
#endif
#if PLATFORM_SWITCH
#include <nn/nn_Log.h>
#endif
#ifndef _MAX_PATH
#define _MAX_PATH 500
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cerrno>

#ifndef _MSC_VER
#define __stdcall
#endif
typedef void (__stdcall *DebugOutputCallback)(const char *);

class vsBufferedStringStreamBuf : public std::streambuf
{
public:
	vsBufferedStringStreamBuf(int bufferSize) 
	{
		if (bufferSize)
		{
			char *ptr = new char[bufferSize];
			setp(ptr, ptr + bufferSize);
		}
		else
			setp(0, 0);
	}
	virtual ~vsBufferedStringStreamBuf() 
	{
		//sync();
		delete[] pbase();
	}
	virtual void writeString(const std::string &str) = 0;
private:
	int	overflow(int c)
	{
		sync();

		if (c != EOF)
		{
			if (pbase() == epptr())
			{
				std::string temp;
				temp += char(c);
				writeString(temp);
			}
			else
				sputc((char)c);
		}

		return 0;
	}

	int	sync()
	{
		if (pbase() != pptr())
		{
			int len = int(pptr() - pbase());
			std::string temp(pbase(), len);
			writeString(temp);
			setp(pbase(), epptr());
		}
		return 0;
	}
};

class VisualStudioDebugOutput : public vsBufferedStringStreamBuf
{
public:
    VisualStudioDebugOutput(bool send_to_output_window=true,
							const char *logfilename=NULL,size_t bufsize=(size_t)16
							,DebugOutputCallback c=NULL)
		:vsBufferedStringStreamBuf((int)bufsize)
		,to_logfile(false)
		,old_cout_buffer(NULL)
		,old_cerr_buffer(NULL)
		,callback(c)
	{
		//if(errno!=0)
		//	simul::base::RuntimeError(strerror(errno));
		to_output_window=send_to_output_window;
		if(logfilename)
			setLogFile(logfilename);
		old_cout_buffer=std::cout.rdbuf(this);
		old_cerr_buffer=std::cerr.rdbuf(this);
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
	std::string fn;
	void setLogFile(const char *logfilename)
	{
		fn=logfilename;
		if(fn.find(":")>=fn.length())
		{
#ifndef _XBOX_ONE
			char buffer[_MAX_PATH];
			if(_getcwd(buffer,_MAX_PATH))
			{
				fn=buffer;
			}
#endif
			fn+="/";
			fn+=logfilename;
		}
		logFile.open(fn.c_str());
		if(logFile.good())
			to_logfile=true;
		else if(errno!=0)
		{
			std::cerr<<"Failed to create logfile "<<fn.c_str()<<std::endl;
			errno=0;
		}
	}
	void setCallback(DebugOutputCallback c)
	{
		callback=c;
	}
    virtual void writeString(const std::string &str)
    {
		if(to_logfile)
			logFile<<str.c_str();
		if(callback)
		{
			callback(str.c_str());
		}
		if(to_output_window)
		{
#if PLATFORM_SWITCH
			static std::string switch_str;
			switch_str+=str;
			size_t ret_pos=std::min(switch_str.find("\n"),switch_str.find("\r"));
			if(ret_pos<switch_str.length())
			{
				NN_LOG("%s\n",switch_str.substr(0,ret_pos).c_str());
				switch_str=switch_str.substr(ret_pos+1,switch_str.length()-ret_pos-1);
			}
#elif defined(UNICODE)
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
	DebugOutputCallback callback;
};