#ifdef _MSC_VER
#include <windows.h>
#pragma warning(disable:4748)
#endif

#include "Platform/Core/StringToWString.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace platform
{
	namespace core
	{
		std::wstring StringToWString(const std::string &text)
		{
			size_t origsize = strlen(text.c_str()) + 1;
			const size_t newsize = origsize;
			wchar_t *wcstring=new wchar_t[newsize+2];
#ifdef _MSC_VER
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, origsize, text.c_str(), _TRUNCATE);
#else
			mbstowcs(wcstring,text.c_str(),origsize);
#endif
			std::wstring str(wcstring);
			delete [] wcstring;
			return str;
		}
		std::wstring Utf8ToWString(const char *src_utf8)
		{
			int src_length=(int)strlen(src_utf8);
#ifdef _MSC_VER
			int length = MultiByteToWideChar(CP_UTF8, 0, src_utf8,src_length, 0, 0);
#else
			int length=src_length;
#endif
			wchar_t *output_buffer = new wchar_t [length+1];
#ifdef _MSC_VER
			MultiByteToWideChar(CP_UTF8, 0, src_utf8, src_length, output_buffer, length);
#else
			mbstowcs(output_buffer, src_utf8, (size_t)length );
#endif
			output_buffer[length]=0;
			std::wstring wstr=std::wstring(output_buffer);
			delete [] output_buffer;
			return wstr;
		}

		std::wstring Utf8ToWString(std::string utf8)
		{
			return Utf8ToWString(utf8.c_str());
		}

		std::string WStringToUtf8(const wchar_t *src_w)
		{
			int src_length=(int)wcslen(src_w);
#ifdef _MSC_VER
			int size_needed = WideCharToMultiByte(CP_UTF8, 0,src_w, (int)src_length, NULL, 0, NULL, NULL);
#else
			int size_needed=2*src_length;
#endif
			char *output_buffer = new char [size_needed+1];
#ifdef _MSC_VER
			WideCharToMultiByte (CP_UTF8,0,src_w,(int)src_length,output_buffer, size_needed, NULL, NULL);
#else
			wcstombs(output_buffer, src_w, (size_t)size_needed );
#endif
			output_buffer[size_needed]=0;
			std::string str_utf8=std::string(output_buffer);
			delete [] output_buffer;
			return str_utf8;
		}
		std::string WStringToString(const std::wstring &text)
		{
			size_t origsize = text.length()+ 1;
			const size_t newsize = origsize;
			char *cstring=new char[newsize];
			
#ifdef _MSC_VER
			size_t convertedChars = 0;
			wcstombs_s(&convertedChars, cstring, (size_t)origsize, text.c_str(), (size_t)newsize );
#else
			wcstombs(cstring, text.c_str(), (size_t)newsize );
#endif
			std::string str;
			str=std::string(cstring);
			delete [] cstring;
			return str;
		}
		
		std::string WStringToUtf8(std::wstring wstr)
		{
			const wchar_t *src_w=wstr.c_str();
			return WStringToUtf8(src_w);
		}
		void WStringToAsciiString(char *output,const std::wstring &text)
		{
			std::string str=WStringToString(text);
#ifdef _MSC_VER
			strcpy_s(output,str.length(),str.c_str());
#else
			strcpy(output,str.c_str());
#endif
		}
	}
}
