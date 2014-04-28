
#include "DX11Exception.h"

#if WINVER>=0x0602
#include <string>
extern const char *DXGetErrorStringA(HRESULT hr)
{
	static std::string str;
	char *lpBuf;
	DWORD res=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, 0, (LPTSTR)&lpBuf, 0, NULL);
	if(lpBuf)
		str=lpBuf;
	LocalFree(lpBuf);
	return str.c_str();
}
extern const char *DXGetErrorDescriptionA(HRESULT hr)
{
	static std::string str;
	char *lpBuf;
	DWORD res=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, 0, (LPTSTR)&lpBuf, 0, NULL);
	if(lpBuf)
		str=lpBuf;
	LocalFree(lpBuf);
	return str.c_str();
}
#endif