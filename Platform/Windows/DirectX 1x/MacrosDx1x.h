#ifndef MACROS_H_DONE
#define MACROS_H_DONE
#include <iostream>
#include <tchar.h>
typedef std::basic_string<TCHAR> tstring;
#ifdef UNICODE
	#define stprintf_s swprintf_s
#else
	#define stprintf_s sprintf_s
#endif

#ifdef DX10
	#define ID3D1xDevice				ID3D10Device
	#define ID3D1xDeviceContext			ID3D10Device	
	#define ID3D1xBuffer				ID3D10Buffer		
	#define ID3D1xInputLayout			ID3D10InputLayout
	#define ID3D1xResource				ID3D10Resource
	#define ID3D1xTexture1D				ID3D10Texture1D 
	#define ID3D1xTexture2D				ID3D10Texture2D 
	#define ID3D1xTexture3D				ID3D10Texture3D 
	#define ID3D1xQuery					ID3D10Query
	#define ID3D1xEffect				ID3D10Effect
	#define ID3D1xEffectPass			ID3D10EffectPass
	#define ID3D1xEffectTechnique		ID3D10EffectTechnique	
	#define ID3D1xEffectMatrixVariable			ID3D10EffectMatrixVariable
	#define ID3D1xEffectVectorVariable			ID3D10EffectVectorVariable
	#define ID3D1xEffectScalarVariable			ID3D10EffectScalarVariable
	#define ID3D1xShaderResourceView			ID3D10ShaderResourceView
	#define ID3D1xEffectShaderResourceVariable	ID3D10EffectShaderResourceVariable
	#define ID3D1xEffectConstantBuffer			ID3D10EffectConstantBuffer
	#define D3D1xCalcSubresource				D3D10CalcSubresource
	#define D3D1x_USAGE_DEFAULT					D3D10_USAGE_DEFAULT
	#define D3D1x_MAPPED_TEXTURE3D				D3D10_MAPPED_TEXTURE3D
	#define D3D1x_SHADER_RESOURCE_VIEW_DESC		D3D10_SHADER_RESOURCE_VIEW_DESC
	
	#define D3D1x_SRV_DIMENSION_TEXTURE3D				D3D10_SRV_DIMENSION_TEXTURE3D
	#define D3D1x_INPUT_ELEMENT_DESC					D3D10_INPUT_ELEMENT_DESC
	#define D3D1x_QUERY_DESC							D3D10_QUERY_DESC
	#define D3D1x_QUERY_OCCLUSION						D3D10_QUERY_OCCLUSION
	#define D3D1x_INPUT_PER_VERTEX_DATA					D3D10_INPUT_PER_VERTEX_DATA
	#define D3D1x_PASS_DESC								D3D10_PASS_DESC
	#define D3D1x_BUFFER_DESC							D3D10_BUFFER_DESC
	#define D3D1x_USAGE_DYNAMIC							D3D10_USAGE_DYNAMIC
 	#define D3D1x_BIND_VERTEX_BUFFER					D3D10_BIND_VERTEX_BUFFER
	#define D3D1x_CPU_ACCESS_WRITE						D3D10_CPU_ACCESS_WRITE
	#define D3D1x_SUBRESOURCE_DATA							D3D10_SUBRESOURCE_DATA
	#define D3D1x_TEXTURE2D_DESC							D3D10_TEXTURE2D_DESC
	#define D3D1x_TEXTURE3D_DESC							D3D10_TEXTURE3D_DESC
	#define D3D1x_MAPPED_TEXTURE2D							D3D10_MAPPED_TEXTURE2D 
	#define D3D1x_MAPPED_TEXTURE1D							D3D10_MAPPED_TEXTURE2D 
	#define D3D1x_MAP_WRITE_DISCARD							D3D10_MAP_WRITE_DISCARD 
	#define D3D1x_TEXTURE1D_DESC							D3D10_TEXTURE1D_DESC
	#define D3D1x_BIND_SHADER_RESOURCE						D3D10_BIND_SHADER_RESOURCE
	#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP			D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST			D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	#define D3DX1xCreateTextureFromFile						D3DX10CreateTextureFromFile
	#define D3DX1x_IMAGE_LOAD_INFO							D3DX10_IMAGE_LOAD_INFO
	#define ID3D1xRenderTargetView							ID3D10RenderTargetView
	#define ID3D1xDepthStencilView							ID3D10DepthStencilView
	#define D3D1x_BIND_RENDER_TARGET						D3D10_BIND_RENDER_TARGET
	#define D3D1x_BIND_DEPTH_STENCIL						D3D10_BIND_DEPTH_STENCIL
	#define D3D1x_CLEAR_DEPTH								D3D10_CLEAR_DEPTH
	#define D3D1x_CLEAR_STENCIL								D3D10_CLEAR_STENCIL

#else
	#define ID3D1xDevice				ID3D11Device
	#define ID3D1xDeviceContext			ID3D11DeviceContext	
	#define ID3D1xBuffer				ID3D11Buffer		
	#define ID3D1xInputLayout			ID3D11InputLayout
	#define ID3D1xResource				ID3D11Resource
	#define ID3D1xTexture1D				ID3D11Texture1D
	#define ID3D1xTexture2D						ID3D11Texture2D
	#define ID3D1xTexture3D						ID3D11Texture3D
	#define ID3D1xQuery							ID3D11Query
	#define ID3D1xEffect						ID3DX11Effect
	#define ID3D1xEffectPass					ID3DX11EffectPass
	#define ID3D1xEffectTechnique				ID3DX11EffectTechnique	
	#define ID3D1xEffectMatrixVariable			ID3DX11EffectMatrixVariable
	#define ID3D1xEffectVectorVariable			ID3DX11EffectVectorVariable
	#define ID3D1xEffectScalarVariable			ID3DX11EffectScalarVariable
	#define ID3D1xEffectShaderResourceVariable	ID3DX11EffectShaderResourceVariable
	#define ID3D1xEffectConstantBuffer			ID3DX11EffectConstantBuffer
	#define D3D1xCalcSubresource				D3D11CalcSubresource
	#define ID3D1xShaderResourceView			ID3D11ShaderResourceView
	#define D3D1x_USAGE_DEFAULT					D3D11_USAGE_DEFAULT
	#define D3D1x_MAPPED_TEXTURE3D				D3D11_MAPPED_SUBRESOURCE
	#define D3D1x_SHADER_RESOURCE_VIEW_DESC		D3D11_SHADER_RESOURCE_VIEW_DESC

	#define D3D1x_SRV_DIMENSION_TEXTURE3D		D3D11_SRV_DIMENSION_TEXTURE3D
	#define D3D1x_INPUT_ELEMENT_DESC			D3D11_INPUT_ELEMENT_DESC
	#define D3D1x_QUERY_DESC					D3D11_QUERY_DESC
	#define D3D1x_QUERY_OCCLUSION				D3D11_QUERY_OCCLUSION
	#define D3D1x_INPUT_PER_VERTEX_DATA			D3D11_INPUT_PER_VERTEX_DATA
	#define D3D1x_PASS_DESC						D3DX11_PASS_DESC
	#define D3D1x_BUFFER_DESC					D3D11_BUFFER_DESC
	#define D3D1x_USAGE_DYNAMIC					D3D11_USAGE_DYNAMIC
 	#define D3D1x_BIND_VERTEX_BUFFER			D3D11_BIND_VERTEX_BUFFER
	#define D3D1x_CPU_ACCESS_WRITE				D3D11_CPU_ACCESS_WRITE
	#define D3D1x_SUBRESOURCE_DATA				D3D11_SUBRESOURCE_DATA
	#define D3D1x_TEXTURE2D_DESC				D3D11_TEXTURE2D_DESC
	#define D3D1x_TEXTURE3D_DESC				D3D11_TEXTURE3D_DESC
	#define D3D1x_MAPPED_TEXTURE2D				D3D11_MAPPED_SUBRESOURCE
	#define D3D1x_MAPPED_TEXTURE1D				D3D11_MAPPED_SUBRESOURCE
	#define D3D1x_MAP_WRITE_DISCARD				D3D11_MAP_WRITE_DISCARD 
	#define D3D1x_TEXTURE1D_DESC				D3D11_TEXTURE1D_DESC
	#define D3D1x_BIND_SHADER_RESOURCE			D3D11_BIND_SHADER_RESOURCE
	#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	#define D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLELIST			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	#define D3DX1xCreateTextureFromFile						D3DX11CreateTextureFromFile
	#define D3DX1x_IMAGE_LOAD_INFO							D3DX11_IMAGE_LOAD_INFO
	#define ID3D1xRenderTargetView							ID3D11RenderTargetView
	#define ID3D1xDepthStencilView							ID3D11DepthStencilView
	#define D3D1x_BIND_RENDER_TARGET						D3D11_BIND_RENDER_TARGET
	#define D3D1x_BIND_DEPTH_STENCIL						D3D11_BIND_DEPTH_STENCIL
	#define D3D1x_CLEAR_DEPTH								D3D11_CLEAR_DEPTH
	#define D3D1x_CLEAR_STENCIL								D3D11_CLEAR_STENCIL

#endif

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
#if 0//def UNICODE

	#define WIDEN2(x) L ## x
	#define WIDEN(x) WIDEN2(x)
	#define __WFILE__ WIDEN(__FILE__)
	#define WIDEN4(x) _T(#x)
	#define WIDEN3(x) WIDEN4(x)
	#define __WLINE__ WIDEN3(__LINE__)
	#define WIDENSTRING(x) L#x

	#define BreakIfDebugging()

	#ifndef V_RETURN
		#define V_RETURN(x)	{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_RETURN error %d at file %s, line %d",(int)hr,__WFILE__,__LINE__);std::cerr<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();return hr; } }
	#endif
	#ifndef V_CHECK
		#define V_CHECK(x)	{ hr = x; if( FAILED(hr) ) {wchar_t text[200];wsprintf(text,L"V_CHECK error %d at file %s, line %d",(int)hr,__WFILE__,__LINE__);std::cerr<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging();} }
	#endif
	#ifndef V_FAIL
		#define V_FAIL(msg)	{ wchar_t text[200];wsprintf(text,_T("V_FAIL: %s - file %s, line %d"),WIDENSTRING(msg),__WFILE__,__WLINE__);std::cerr<<text<<std::endl;MessageBox(NULL,text,L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);BreakIfDebugging(); }
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