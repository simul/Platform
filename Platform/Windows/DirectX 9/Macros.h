#ifndef MACROS_H_DONE
	#define MACROS_H_DONE
	#include <iostream>
#include <tchar.h>
	#define PIXBeginNamedEvent(a,b) //D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent()		// D3DPERF_EndEvent()

	#ifndef SAFE_RELEASE
		#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
	#endif
#ifdef _UNICODE

	#define WIDEN2(x) L ## x
	#define WIDEN(x) WIDEN2(x)
	#define __WFILE__ WIDEN(__FILE__)
	#define WIDENSTRING(x) L#x


	#ifndef V_RETURN
#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_RETURN error %d at file %s, line %d",hr,__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);DebugBreak();return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_CHECK error %d at file %s, line %d",hr,__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);DebugBreak();} }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ wchar_t text[200];wsprintf(text,L"V_FAIL: %s - file %s, line %d",WIDENSTRING(msg),__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);DebugBreak(); }
	#endif

#else
	#ifndef V_RETURN
		#define V_RETURN(x)	{ hr = x; if( FAILED(hr) ) {std::cerr<<"V_RETURN error "<<hr<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl;return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{ hr = x; if( FAILED(hr) ) {std::cerr<<"V_RETURN error "<<hr<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl; } }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ std::cerr<<"V_RETURN error "<<msg<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl; DebugBreak(); }
	#endif
#endif
	#ifndef SAFE_DELETE
		#define SAFE_DELETE(p)		{ if(p) { delete (p);     (p)=NULL; } }
	#endif
#endif