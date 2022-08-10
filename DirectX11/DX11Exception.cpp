
#include "DX11Exception.h"

#ifdef SIMUL_WIN8_SDK
#include <string>
extern const char *DXGetErrorStringA(HRESULT hr)
{
	static std::string str;
	char *lpBuf=nullptr;
	DWORD res= FormatMessageA(
#ifndef _XBOX_ONE
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
#endif
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, 0, (LPSTR)&lpBuf, 0, NULL);
	if(lpBuf)
		str=lpBuf;
#ifndef _XBOX_ONE
	LocalFree(lpBuf);
#endif
	return str.c_str();
}
extern const char *DXGetErrorDescriptionA(HRESULT hr)
{
	static std::string str;
	char *lpBuf = nullptr;
	DWORD res= FormatMessageA(
#ifndef _XBOX_ONE
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
#endif
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, 0, (LPSTR)&lpBuf, 0, NULL);
	if(lpBuf)
		str=lpBuf;
#ifndef _XBOX_ONE
	LocalFree(lpBuf);
#endif
	return str.c_str();
}
#endif