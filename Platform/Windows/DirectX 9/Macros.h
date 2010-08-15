#ifndef MACROS_H_DONE
#define MACROS_H_DONE
#include <iostream>
#include <tchar.h>
#ifdef ENABLE_PIX
	#define PIXBeginNamedEvent(a,b) D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent()		D3DPERF_EndEvent()
	#define PIXWrapper(a,b)			PIXBeginNamedEvent(a,b);for(int pixw=0;pixw<1;pixw++,PIXEndNamedEvent())
#else
	#define PIXBeginNamedEvent(a,b) //D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent()		// D3DPERF_EndEvent()
	#define PIXWrapper(a,b)
#endif
	#ifndef SAFE_RELEASE
		#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
	#endif
#ifdef UNICODE

	#define WIDEN2(x) L ## x
	#define WIDEN(x) WIDEN2(x)
	#define __WFILE__ WIDEN(__FILE__)
	#define WIDENSTRING(x) L#x

	#define BreakIfDebugging()

	#ifndef V_RETURN
		#define V_RETURN(x)			{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_RETURN error %d at file %s, line %d",hr,__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_CHECK error %d at file %s, line %d",hr,__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();} }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ wchar_t text[200];wsprintf(text,L"V_FAIL: %s - file %s, line %d",WIDENSTRING(msg),__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging(); }
	#endif

#else
	#ifndef V_RETURN
		#define V_RETURN(x)	{ hr = x; if( FAILED(hr) ) {std::cerr<<"V_RETURN error "<<hr<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl;return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{ hr = x; if( FAILED(hr) ) {std::cerr<<"V_RETURN error "<<hr<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl; } }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ ::cerr<<"V_RETURN error "<<msg<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl; BreakIfDebugging(); }
	#endif
#endif
	#ifndef SAFE_DELETE
		#define SAFE_DELETE(p)		{ if(p) { delete (p);     (p)=NULL; } }
	#endif
	#ifndef SAFE_DELETE_SMARTPTR
		#define SAFE_DELETE_SMARTPTR(p)		{ if(p.get()) { (p)=NULL; } }
	#endif
#endif