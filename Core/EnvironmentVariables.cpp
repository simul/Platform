
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"

#if !defined(__GNUC__)
#include <direct.h>
#endif 
#include <errno.h>
#include <iostream>
#include <algorithm>
#include <time.h>
#include <string.h>// for strlen

#if defined(WIN32)|| defined(WIN64)
#include <windows.h>
#pragma warning(disable:4748)
#endif
#ifndef MAX_PATH
#define MAX_PATH 500
#endif

using namespace platform;
using namespace core;

namespace platform
{
	namespace core
	{
		static bool useExternalTextures=false;
		void SetUseExternalTextures(bool t)
		{
			useExternalTextures=t;
		}
		bool GetUseExternalTextures()
		{
			return useExternalTextures;
		}
	}
}
#include <stdio.h>
#include <stdlib.h>
std::string EnvironmentVariables::GetSimulEnvironmentVariable(const std::string &name_utf8)
{
#if defined(WIN32)|| defined(WIN64)
	std::wstring wname=Utf8ToWString(name_utf8);
	size_t sizeNeeded;
	wchar_t buffer[2];
	errno_t err=_wgetenv_s(&sizeNeeded,
				buffer,
				1,
				wname.c_str());
	std::wstring wret=L"";
	if(err!=0&&sizeNeeded>0)
	{
		wchar_t *txt=new wchar_t[sizeNeeded];
		err=_wgetenv_s(&sizeNeeded,
				txt,
				sizeNeeded,
				wname.c_str()); 
		if(err!=EINVAL)
			wret=(txt);
		delete [] txt;
	}
	std::string ret=WStringToUtf8(wret);
	return ret;
#elif defined(UNIX)
	std::string ret;
	//size_t len;
	//char value[4097];
	const char *value=getenv(name_utf8.c_str());
	//auto err=getenv_s(&len,value,4096, name_utf8.c_str());
	if(value)
		ret=value;
	return ret;
#else
	return std::string("");
#endif
}
std::string EnvironmentVariables::SetSimulEnvironmentVariable(const std::string &name,const std::string &value)
{
#if defined(WIN32)|| defined(WIN64)
	std::string str=name+"=";
	str+=value;
	_putenv(str.c_str());
#endif
	return value;
}

std::string EnvironmentVariables::GetWorkingDirectory()
{
	std::string str;
#if defined(WIN32)|| defined(WIN64)
	char buffer[_MAX_PATH];
	if(_getcwd(buffer,_MAX_PATH))
	{
		str=buffer;
	}
	else
		str="";
#endif
	return str;
}

std::string EnvironmentVariables::GetExecutableDirectory()
{
	std::string str;
#if defined(WIN32)|| defined(WIN64)
	char filename[_MAX_PATH];
	if(GetModuleFileNameA(NULL,filename,_MAX_PATH))
	{
		str=filename;
		int pos=(int)str.find_last_of('/');
		int back=(int)str.find_last_of('\\');
		if(back>pos)
			pos=back;
		str=str.substr(0,pos);
	}
	else
		str="";
#elif defined(__LINUX__)
//Linux:
	char pBuf[512];
	size_t len = sizeof(pBuf);
	int bytes = MIN(readlink("/proc/self/exe", pBuf, len), len - 1);
	if (bytes >= 0)
		pBuf[bytes] = '\0';
	str = pBuf;
#endif
	return str;
}

void EnvironmentVariables::SetWorkingDirectory(std::string dir)
{
#if defined(WIN32)|| defined(WIN64)
	_chdir(dir.c_str());
#endif
}

std::string EnvironmentVariables::AppendSimulEnvironmentVariable(const std::string &name,const std::string &value)
{
	std::string str=name+"=";
#if defined(WIN32)|| defined(WIN64)
	value;
	str+=value;
		str+=";";
	char txt[200];
	size_t sz=0;

	errno_t err=getenv_s( &sz,
					   txt,
					   199,
					   name.c_str() 
					);

	str+=txt;
	_putenv(str.c_str());
#endif
	return str;
}
