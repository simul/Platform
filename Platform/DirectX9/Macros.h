#ifndef MACROS_H_DONE
	#define MACROS_H_DONE
	#include <iostream>
	#include <tchar.h>
	#include <comdef.h>
	#pragma comment(lib,"dxerr")
	#pragma comment(lib,"d3dx9")
	#pragma comment(lib,"d3d9")
	#pragma comment(lib,"d3dx10")
	#pragma comment(lib,"comctl32")
	typedef std::basic_string<TCHAR> tstring;
	#define ERROR_CHECK
	#ifdef UNICODE
		#define stprintf_s swprintf_s
	#else
		#define stprintf_s sprintf_s
	#endif
	extern const TCHAR *GetErrorText(HRESULT hrx);

	#ifdef ENABLE_PIX
		#define PIXBeginNamedEvent(colour,name) //D3DPERF_BeginEvent(colour,L##name)
		#define PIXEndNamedEvent()				//D3DPERF_EndEvent()
		#define PIXWrapper(colour,name)			D3DPERF_BeginEvent(colour,L##name);for(int pixw=0;pixw<1;pixw++,D3DPERF_EndEvent())
	#else
		#define PIXBeginNamedEvent(colour,name) //D3DPERF_BeginEvent(colour,L##name)
		#define PIXEndNamedEvent()				// D3DPERF_EndEvent()
		#define PIXWrapper(colour,name)
	#endif
	#ifndef SAFE_RELEASE
		#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
	#endif

	#define BreakIfDebugging()
		
	#ifdef UNICODE
		#define WIDEN2(x) L ## x
		#define WIDEN(x) WIDEN2(x)
		#define __WFILE__ WIDEN(__FILE__)
		#define __TFILE__ __WFILE__
		#define WIDEN4(x) _T(#x)
		#define WIDEN3(x) WIDEN4(x)
		#define __WLINE__ WIDEN3(__LINE__)
		#define WIDENSTRING(x) L#x
	#else
		#define __TFILE__ __FILE__
	#endif

	#ifndef B_RETURN
		#define B_RETURN(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {TCHAR text[200];_stprintf(text,_T("V_RETURN error %d at file %s, line %d"),(int)hrx,__TFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,_T("ERROR"), MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return false; } }
	#endif
		
	#ifdef UNICODE

		#ifndef VOID_RETURN
			#define VOID_RETURN(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {wchar_t text[200];wsprintf(text,L"V_RETURN error %s at file %s, line %d",GetErrorText(hrx),__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return; } }
		#endif
		#ifndef V_RETURN
			#define V_RETURN(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {wchar_t text[200];wsprintf(text,L"V_RETURN error %s at file %s, line %d",GetErrorText(hrx),__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return hrx; } }
		#endif
		#ifndef V_CHECK
			#define V_CHECK(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {wchar_t text[200];wsprintf(text,L"V_CHECK error %s at file %s, line %d",GetErrorText(hrx),__WFILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();} }
		#endif
		#ifndef V_FAIL
			#define V_FAIL(msg)	{ wchar_t text[200];wsprintf(text,_T("V_FAIL: %s - file %s, line %d"),WIDENSTRING(msg),__WFILE__,__WLINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging(); }
		#endif

	#else
		#ifndef VOID_RETURN
			#define VOID_RETURN(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {char text[200];sprintf_s(text,199,"V_RETURN error %s at file %s, line %d",GetErrorText(hrx),__FILE__,__LINE__);std::cout<<text<<std::endl;MessageBox(NULL,text,"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return; } }
		#endif
		#ifndef V_RETURN
			#define V_RETURN(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {std::cerr<<"V_RETURN error "<<GetErrorText(hrx)<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl;BreakIfDebugging(); return hrx; } }
		#endif
		#ifndef V_CHECK
			#define V_CHECK(x)	{ HRESULT hrx=x; if( FAILED(hrx) ) {std::cerr<<"V_RETURN error "<<GetErrorText(hrx)<<" at file "<<__FILE__<<" line "<<__LINE__<<std::endl;BreakIfDebugging(); } }
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

	// Define IDirect3DTexture9Ptr and so on:
	_COM_SMARTPTR_TYPEDEF(IDirect3DSurface9, __uuidof(IDirect3DSurface9));
	_COM_SMARTPTR_TYPEDEF(IDirect3DTexture9, __uuidof(IDirect3DTexture9));
	_COM_SMARTPTR_TYPEDEF(IDirect3DVertexDeclaration9, __uuidof(IDirect3DVertexDeclaration9));
	
#endif