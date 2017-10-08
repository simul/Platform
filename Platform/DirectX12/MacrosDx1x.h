#ifndef MACROS12_H_DONE
#define MACROS12_H_DONE

#include <iostream>
#include <tchar.h>
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Macros.h"

typedef std::basic_string<TCHAR> tstring;

#define ENABLE_PIX
#ifdef UNICODE
	#define stprintf_s swprintf_s
#else
	#define stprintf_s sprintf_s
#endif

typedef long HRESULT;

// SIMUL_HELP
// Here we define a compile-time assert for type-checking:
#define COMPILE_ASSERT(x) extern int __dummy[(int)x]
#define VERIFY_EXPLICIT_CAST(from, to) COMPILE_ASSERT(sizeof(from) == sizeof(to)) 

extern const char *GetErrorText(HRESULT hr);
#ifdef ENABLE_PIX
	#define PIXBeginNamedEvent(colour,name) D3DPERF_BeginEvent(colour,L##name)
	#define PIXEndNamedEvent()				D3DPERF_EndEvent()
	#define PIXWrapper(colour,name)			PIXBeginNamedEvent(colour,name);for(int pixw=0;pixw<1;pixw++,PIXEndNamedEvent())
#else
	#define PIXBeginNamedEvent(colour,name)
	#define PIXEndNamedEvent()				
	#define PIXWrapper(colour,name)
#endif

#ifndef SAFE_RELEASE
	#define SAFE_RELEASE(p)			{ if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE_ARRAY
	#define SAFE_RELEASE_ARRAY(p,n)	{ if(p) for(int i=0;i<n;i++) if(p[i]) { (p[i])->Release(); (p[i])=NULL; } }
#endif

#endif
