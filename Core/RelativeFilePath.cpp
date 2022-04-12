#include "Platform/Core/RelativeFilePath.h"
#include <queue>
#include <stack>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
	// NOTE (trent, 7/25/17): I'm really quite unsure about this change; it appears that rsize_t is assumed to be different on some platforms. This gets past the errors, but, again: unsure.
	#define rsize_t size_t
#endif

using namespace std;
typedef queue<char*> QueuePtrChar;
typedef stack<char*> StackPtrChar;
#ifndef MAX_PATH
#define MAX_PATH 500
#endif
#if defined(_MSC_VER)
	char *strtok_s(char * str, rsize_t * strmax, const char * delim, char ** ptr)
	{
		return strtok_s(str,delim,ptr);
	}
	#define path_separator "\\"
	#define path_sep_char  '\\'
#elif defined(__ORBIS__)||defined(UNIX)||defined(__COMMODORE__)
	#define path_separator "/"
    #define path_sep_char  '/'

char *strtok_s(char * str, size_t * , const char * delim, char ** )
{
    return strtok(str,delim);
}
#else
	#error define your compiler
#endif

// Macro off the string buffer manipulation functions.
//	NOTE (trent, 7/25/17): this will likely need a linux equivalent.
#ifndef _MSC_VER
    #if defined(UNIX)
        #define _strcpy(d,n,s) (strncpy(d,s,n))
    #else //__APPLE__
        #define _strcpy(d,n,s)	(strlcpy(d,s,n))

        #define strtok_s strtok_r
        char *strtok_s(char * str, size_t * strmax, const char * delim, char ** ptr)
        {
            return strtok_s(str,delim,ptr);
        }
    #endif
#else
	#define _strcpy(d,n,s)	(strcpy_s(d,n,s))
#endif	// __APPLE__.

std::string platform::core::Abs2Rel(const std::string &pcAbsPath,const std::string &pcCurrDir)
{
	char pcRelPath[MAX_PATH];
	char acTmpCurrDir[MAX_PATH];
	char acTmpAbsPath[MAX_PATH];
	_strcpy(acTmpCurrDir,MAX_PATH-1,pcCurrDir.c_str());
	_strcpy(acTmpAbsPath,MAX_PATH-1,pcAbsPath.c_str());
	
	StackPtrChar tmpStackAbsPath;
	StackPtrChar tmpStackCurrPath;
	StackPtrChar tmpStackOutput;
	QueuePtrChar tmpMatchQueue;
	char *next_token= NULL;
	
    size_t strmax = sizeof acTmpAbsPath;
	char *sTmp = strtok_s(acTmpAbsPath,&strmax,path_separator, &next_token);
	while(sTmp)
	{
		tmpStackAbsPath.push(sTmp);
		sTmp = strtok_s(0, &strmax,path_separator, &next_token);
	}

	sTmp = strtok_s(acTmpCurrDir,&strmax,path_separator, &next_token);
	while(sTmp)
	{
		tmpStackCurrPath.push(sTmp);
		sTmp = strtok_s(0, &strmax,path_separator, &next_token);
	}

	sTmp = pcRelPath;
	while(tmpStackCurrPath.size() > tmpStackAbsPath.size() )
	{
		*sTmp++ = '.';
		*sTmp++ = '.';
		*sTmp++ = path_sep_char;
		tmpStackCurrPath.pop();
	}

	while(tmpStackAbsPath.size() > tmpStackCurrPath.size() )
	{
		char *pcTmp = tmpStackAbsPath.top();
		tmpStackOutput.push(pcTmp);
		tmpStackAbsPath.pop();
	}

	while(tmpStackAbsPath.size() > 0)
	{
#if !defined(SN_TARGET_PS3) && !defined(__GNUC__)
		if(_stricmp(tmpStackAbsPath.top(),tmpStackCurrPath.top())== 0  )
#else
		if(strcmp(tmpStackAbsPath.top(),tmpStackCurrPath.top())== 0  )
#endif
			tmpMatchQueue.push(tmpStackAbsPath.top());
		else
		{
			while(tmpMatchQueue.size() > 0)
			{
				tmpStackOutput.push(tmpMatchQueue.front());
				tmpMatchQueue.pop();
			}
			tmpStackOutput.push(tmpStackAbsPath.top());
			*sTmp++ = '.';
			*sTmp++ = '.';
			*sTmp++ = path_sep_char;
		}
		tmpStackAbsPath.pop();
		tmpStackCurrPath.pop();	
	}
	while(tmpStackOutput.size() > 0)
	{
		char *pcTmp= tmpStackOutput.top();
		while(*pcTmp != '\0')	
			*sTmp++ = *pcTmp++;
		tmpStackOutput.pop();
		*sTmp++ = path_sep_char;
	}
	if(sTmp!=pcRelPath)
		*(--sTmp) = '\0';
	else return "";


	return std::string(pcRelPath);
}
// From one relative path, make another relative to a different directory.
std::string platform::core::Rel2Rel(const std::string &pcFirstRelPath,const std::string &pcRelCurrDir)
{
	// count the depth of each path:
	string fakeCurrDir;
	int pos=0;

	int depth1=0;
	while((pos=(int)pcFirstRelPath.find(path_separator,++pos))>=0)
		depth1++;

	int depth2=0;
	while((pos=(int)pcRelCurrDir.find(path_separator,++pos))>=0)
		depth2++;

	int depth=std::max(depth1,depth2);

	for(int i=0;i<depth;i++)
		fakeCurrDir+="fake\\";

	string pcAbsPath=Rel2Abs(pcFirstRelPath,fakeCurrDir);
	string pcCurrDir=Rel2Abs(pcRelCurrDir,fakeCurrDir);

	return Abs2Rel(pcAbsPath,pcCurrDir);
}

std::string platform::core::Rel2Abs(const std::string &pcRelPath,const std::string &pcCurrDir)
{
	char acTmpCurrDir[MAX_PATH];
	char acTmpRelPath[MAX_PATH];
	_strcpy(acTmpCurrDir,MAX_PATH-1,pcCurrDir.c_str());
	_strcpy(acTmpRelPath,MAX_PATH-1,pcRelPath.c_str());
	
	QueuePtrChar tmpQueueRelPath;
	StackPtrChar tmpStackCurrPath;
	StackPtrChar tmpStackOutPath;
	char *next_token= NULL;
	
    size_t strmax = sizeof acTmpRelPath;
	char *sTmp = strtok_s(acTmpRelPath,&strmax,path_separator, &next_token);
	while(sTmp)
	{
		tmpQueueRelPath.push(sTmp);
		sTmp = strtok_s(0, &strmax,path_separator, &next_token);
	}

	sTmp = strtok_s(acTmpCurrDir,&strmax,path_separator, &next_token);
	while(sTmp)
	{
		tmpStackCurrPath.push(sTmp);
		sTmp = strtok_s(0, &strmax,path_separator, &next_token);
	}


	while(tmpQueueRelPath.size() > 0)
	{
		char *pcTmp= tmpQueueRelPath.front();
		if(strcmp(pcTmp, "..") == 0)
			tmpStackCurrPath.pop();
		else
			tmpStackCurrPath.push(pcTmp);
		tmpQueueRelPath.pop();
	}
	while(tmpStackCurrPath.size() > 0)
	{
		tmpStackOutPath.push(tmpStackCurrPath.top());
		tmpStackCurrPath.pop();
	}


	char pcAbsPath[MAX_PATH];
	sTmp = pcAbsPath;
#if defined(__GNUC__)
	*sTmp++ = path_sep_char;
#endif
	while(tmpStackOutPath.size() > 0)
	{
		char *pcTmp= tmpStackOutPath.top();
		while(*pcTmp != '\0')	
			*sTmp++ = *pcTmp++;
		tmpStackOutPath.pop();
		*sTmp++ = path_sep_char;
	}
	*(--sTmp) = '\0';

	return pcAbsPath; 	
}
