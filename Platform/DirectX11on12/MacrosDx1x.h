#ifndef MACROS_H_DONE
#define MACROS_H_DONE
#include <iostream>
#include <tchar.h>
#include "Simul/Platform/DirectX11on12/Export.h"
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
	#define PIXBeginNamedEvent(colour,name) //D3DPERF_BeginEvent(colour,L##name)
	#define PIXEndNamedEvent()				// D3DPERF_EndEvent()
	#define PIXWrapper(colour,name)
#endif
	#ifndef SAFE_RELEASE
		#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }
	#endif
	#ifndef SAFE_RELEASE_ARRAY
		#define SAFE_RELEASE_ARRAY(p,n)		{ if(p) for(int i=0;i<n;i++) if(p[i]) { (p[i])->Release(); (p[i])=NULL; } }
	#endif

extern void SIMUL_DIRECTX11_EXPORT BreakIfDebugging();
#ifdef UNICODE

	#define WIDEN2(x) L ## x
	#define WIDEN(x) WIDEN2(x)
	#define __WFILE__ WIDEN(__FILE__)
	#define WIDEN4(x) _T(#x)
	#define WIDEN3(x) WIDEN4(x)
	#define __WLINE__ WIDEN3(__LINE__)
	#define WIDENSTRING(x) L#x

	#ifndef B_RETURN//  : error C2065: 'B_RET' : undeclared identifier
		#define B_RETURN(x)	{VERIFY_EXPLICIT_CAST(x,bool); if(!x) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: B_RETURN error, return value is false."<<std::endl;BreakIfDebugging();return false; } }
	#endif
	#ifndef B_CHECK
		#define B_CHECK(x)	{VERIFY_EXPLICIT_CAST(x,bool);if(!x) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: B_CHECK error, return value is false."<<std::endl;BreakIfDebugging();} }
	#endif
	#ifndef V_RETURN
		#define V_RETURN(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);hr = x; if( FAILED(hr) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_RETURN error, return value is  "<<GetErrorText(x)<<std::endl;BreakIfDebugging();return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{HRESULT hrx = x; if( FAILED(hrx) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<GetErrorText(hrx)<<std::endl;BreakIfDebugging(); } }
	#endif
	#ifndef V_CHECK_ONCE
#define V_CHECK_ONCE(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);HRESULT hrx = x; if( FAILED(hrx) ) {static bool failed=false;if(!failed){failed=true;std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<GetErrorText(hrx)<<std::endl;BreakIfDebugging();} } }
	#endif
	#ifndef V_CHECK_RETURN
		#define V_CHECK_RETURN(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);HRESULT hrx = x; if( FAILED(hrx) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<GetErrorText(hrx)<<std::endl;BreakIfDebugging();return; } }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_FAIL error."<<std::endl;BreakIfDebugging();  }
	#endif
#else
	#ifndef B_RETURN//  : error C2065: 'B_RET' : undeclared identifier
		#define B_RETURN(x)	{VERIFY_EXPLICIT_CAST(x,bool); if(!x) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: B_RETURN error, return value is false."<<std::endl;BreakIfDebugging();return false; } }
	#endif
	#ifndef B_CHECK
		#define B_CHECK(x)	{VERIFY_EXPLICIT_CAST(x,bool);if(!x) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: B_CHECK error, return value is false."<<std::endl;BreakIfDebugging();} }
	#endif
	#ifndef V_RETURN
		#define V_RETURN(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);hr = x; if( FAILED(hr) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_RETURN error, return value is  "<<GetErrorText(x)<<std::endl;BreakIfDebugging();return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);hr = x; if( FAILED(hr) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<GetErrorText(x)<<std::endl;BreakIfDebugging(); } }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_FAIL error."<<std::endl;BreakIfDebugging();  }
	#endif
#endif
	
#define MAKE_CONSTANT_BUFFER(cb,type)	\
	{\
		SAFE_RELEASE(cb);	\
		D3D11_SUBRESOURCE_DATA cb_init_data;\
		type x;\
		cb_init_data.pSysMem = &x;\
		cb_init_data.SysMemPitch = 0;\
		cb_init_data.SysMemSlicePitch = 0;\
		D3D11_BUFFER_DESC cb_desc;\
		cb_desc.Usage				= D3D11_USAGE_DYNAMIC;\
		cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;\
		cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;\
		cb_desc.MiscFlags			= 0;\
		cb_desc.ByteWidth			= PAD16(sizeof(type));\
		cb_desc.StructureByteStride = 0;\
		m_pd3dDevice->CreateBuffer(&cb_desc, &cb_init_data, &cb);\
	}

#define UPDATE_CONSTANT_BUFFER(pContext,cb,DataStructType,dataStruct)	\
	{	\
		D3D11_MAPPED_SUBRESOURCE mapped_res;	\
		pContext->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);	\
		*(DataStructType*)mapped_res.pData = dataStruct;	\
		pContext->Unmap(cb, 0);	\
	}

#define UPDATE_CONSTANT_BUFFERS(pContext,cb,DataStructType,dataStructList,num)	\
	{	\
		D3D11_MAPPED_SUBRESOURCE mapped_res;	\
		pContext->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);	\
		for(int i=0;i<num;i++)\
		{	((DataStructType*)(mapped_res.pData))[i]=dataStructList[i];	}\
		pContext->Unmap(cb, 0);	\
	}

#endif