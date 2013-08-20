#ifndef MACROS_H_DONE
#define MACROS_H_DONE
#include <iostream>
#include <tchar.h>
#include "Simul/Platform/DirectX11/Export.h"
typedef std::basic_string<TCHAR> tstring;
#define ENABLE_PIX
#ifdef UNICODE
	#define stprintf_s swprintf_s
#else
	#define stprintf_s sprintf_s
#endif

#define ID3D1xDevice								ID3D11Device
#define ID3D1xBuffer								ID3D11Buffer		
#define ID3D1xInputLayout							ID3D11InputLayout
#define ID3D1xResource								ID3D11Resource
#define ID3D1xTexture1D								ID3D11Texture1D
#define ID3D1xTexture2D								ID3D11Texture2D
#define ID3D1xTexture3D								ID3D11Texture3D
#define ID3D1xQuery									ID3D11Query
#define ID3D1xEffect								ID3DX11Effect
#define ID3D1xEffectPass							ID3DX11EffectPass
#define ID3D1xEffectTechnique						ID3DX11EffectTechnique	
#define ID3D1xEffectMatrixVariable					ID3DX11EffectMatrixVariable
#define ID3D1xEffectVectorVariable					ID3DX11EffectVectorVariable
#define ID3D1xEffectScalarVariable					ID3DX11EffectScalarVariable
#define ID3D1xEffectShaderResourceVariable			ID3DX11EffectShaderResourceVariable
#define ID3D1xEffectConstantBuffer					ID3DX11EffectConstantBuffer
#define D3D1xCalcSubresource						D3D11CalcSubresource
#define ID3D1xShaderResourceView					ID3D11ShaderResourceView
#define ID3D1xBlendState							ID3D11BlendState

#define D3D1x_USAGE_DEFAULT							D3D11_USAGE_DEFAULT
#define D3D1x_MAPPED_TEXTURE3D						D3D11_MAPPED_SUBRESOURCE
#define D3D1x_SHADER_RESOURCE_VIEW_DESC				D3D11_SHADER_RESOURCE_VIEW_DESC
#define D3D1x_DEPTH_STENCIL_VIEW_DESC				D3D11_DEPTH_STENCIL_VIEW_DESC
#define D3D1x_RENDER_TARGET_VIEW_DESC				D3D11_RENDER_TARGET_VIEW_DESC

#define D3D1x_SRV_DIMENSION_TEXTURE3D				D3D11_SRV_DIMENSION_TEXTURE3D
#define D3D1x_SRV_DIMENSION_TEXTURECUBE				D3D11_SRV_DIMENSION_TEXTURECUBE
#define D3D1x_RTV_DIMENSION_TEXTURE2DARRAY			D3D11_RTV_DIMENSION_TEXTURE2DARRAY
#define D3D1x_DSV_DIMENSION_TEXTURE2DARRAY			D3D11_DSV_DIMENSION_TEXTURE2DARRAY

#define D3D1x_INPUT_ELEMENT_DESC					D3D11_INPUT_ELEMENT_DESC
#define D3D1x_QUERY_DESC							D3D11_QUERY_DESC
#define D3D1x_QUERY_OCCLUSION						D3D11_QUERY_OCCLUSION
#define D3D1x_INPUT_PER_VERTEX_DATA					D3D11_INPUT_PER_VERTEX_DATA
#define D3D1x_PASS_DESC								D3DX11_PASS_DESC
#define D3D1x_BUFFER_DESC							D3D11_BUFFER_DESC
#define D3D1x_USAGE_DYNAMIC							D3D11_USAGE_DYNAMIC
#define D3D1x_BIND_VERTEX_BUFFER					D3D11_BIND_VERTEX_BUFFER
#define D3D1x_CPU_ACCESS_WRITE						D3D11_CPU_ACCESS_WRITE 
#define D3D1x_CPU_ACCESS_READ						D3D11_CPU_ACCESS_READ 
#define D3D1x_SUBRESOURCE_DATA						D3D11_SUBRESOURCE_DATA
#define D3D1x_TEXTURE2D_DESC						D3D11_TEXTURE2D_DESC
#define D3D1x_TEXTURE3D_DESC						D3D11_TEXTURE3D_DESC
#define D3D1x_MAPPED_TEXTURE2D						D3D11_MAPPED_SUBRESOURCE
#define D3D1x_MAPPED_TEXTURE1D						D3D11_MAPPED_SUBRESOURCE
#define D3D1x_MAP_WRITE_DISCARD						D3D11_MAP_WRITE_DISCARD 
#define D3D1x_TEXTURE1D_DESC						D3D11_TEXTURE1D_DESC
#define D3D1x_BIND_SHADER_RESOURCE					D3D11_BIND_SHADER_RESOURCE
#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
#define D3D1x_PRIMITIVE_TOPOLOGY_LINESTRIP			D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
#define D3D1x_PRIMITIVE_TOPOLOGY_LINELIST			D3D11_PRIMITIVE_TOPOLOGY_LINELIST
#define D3DX1xCreateTextureFromFile					D3DX11CreateTextureFromFile
#define D3DX1x_IMAGE_LOAD_INFO						D3DX11_IMAGE_LOAD_INFO
#define ID3D1xRenderTargetView						ID3D11RenderTargetView
#define ID3D1xDepthStencilView						ID3D11DepthStencilView
#define D3D1x_BIND_RENDER_TARGET					D3D11_BIND_RENDER_TARGET
#define D3D1x_BIND_DEPTH_STENCIL					D3D11_BIND_DEPTH_STENCIL
#define D3D1x_RESOURCE_MISC_TEXTURECUBE				D3D11_RESOURCE_MISC_TEXTURECUBE
#define D3D1x_RESOURCE_MISC_GENERATE_MIPS			D3D11_RESOURCE_MISC_GENERATE_MIPS
#define D3D1x_CLEAR_DEPTH							D3D11_CLEAR_DEPTH
#define D3D1x_CLEAR_STENCIL							D3D11_CLEAR_STENCIL
#define D3D1x_VIEWPORT								D3D11_VIEWPORT


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
		#define V_CHECK(x)	{VERIFY_EXPLICIT_CAST(x,HRESULT);hr = x; if( FAILED(hr) ) {std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<GetErrorText(x)<<std::endl;BreakIfDebugging(); } }
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