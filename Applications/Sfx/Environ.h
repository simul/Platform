#pragma once
#include <regex>
#include "StringToWString.h"
static std::string GetEnv(const std::string &name_utf8)
{
	std::string ret;
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
	ret=WStringToUtf8(wret);
	return ret;
#else
	const char *s=getenv(name_utf8.c_str());
	if(s)
		ret=s;
	return ret;
#endif
}

static void SetEnv(const std::string &name_utf8, const std::string &value_utf8)
{
#if defined(WIN32)|| defined(WIN64)
	std::wstring wname = Utf8ToWString(name_utf8);
	std::wstring wval = Utf8ToWString(value_utf8);
	errno_t err = _wputenv_s(
		wname.c_str(),wval.c_str());
#else
	setenv(name_utf8.c_str(),value_utf8.c_str(),1);
#endif
}



static std::string ProcessEnvironmentVariables(const std::string &str)
{
	std::string ret=str;
	std::regex re("[\\$%\\!]([A-Z|a-z|0-9|_]+)");
	std::sregex_iterator next(str.begin(), str.end(), re);
	std::sregex_iterator end;
	while (next != end)
	{
		try
		{
			std::smatch match = *next;
			std::string m=match[1].str();
			std::regex re(std::string("[\\$%\\!]")+m);
			ret=std::regex_replace(ret,re,GetEnv(m));
			next++;
		} catch (std::regex_error& err)
		{
			std::cerr << "Regex error: " << err.what() << std::endl;
	  // Syntax error in the regular expression
		}
	}
	return ret;
}