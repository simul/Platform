#pragma once
#ifndef SIMUL_PLATFORM_DIRECTX11_UTILITIES_H
#define SIMUL_PLATFORM_DIRECTX11_UTILITIES_H

#ifdef _XBOX_ONE
#include <d3d11_x.h>
#else
#include "DirectXHeader.h"
#endif
#include <utility>
#include <vector>
#include "Platform/DirectX11/CreateEffectDX1x.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Core/FileLoader.h"
#include "Platform/Math/Matrix4x4.h"
struct ID3DX11EffectConstantBuffer;
#pragma warning(disable:4251)
namespace platform
{
	namespace crossplatform
	{
		struct DeviceContext;
		class RenderPlatform;
	}
	namespace dx11
	{
		extern bool IsTypeless(DXGI_FORMAT fmt, bool partialTypeless);
		extern DXGI_FORMAT TypelessToSrvFormat(DXGI_FORMAT fmt);
		extern DXGI_FORMAT ResourceToDsvFormat(DXGI_FORMAT fmt);
		struct ComputableTexture
		{
			ComputableTexture();
			~ComputableTexture();

			ID3D11Texture2D*            g_pTex_Output;
			ID3D11UnorderedAccessView*  g_pUAV_Output;
			ID3D11ShaderResourceView*   g_pSRV_Output;

			void release();
			void init(ID3D11Device *pd3dDevice,int w,int h);
		};
		struct ArrayTexture
		{
			ArrayTexture()
				:m_pArrayTexture(NULL)
				,m_pArrayTexture_SRV(NULL)
				,unorderedAccessView(NULL)
			{
			}
			~ArrayTexture()
			{
				release();
			}
			void release()
			{
				SAFE_RELEASE(m_pArrayTexture)
				SAFE_RELEASE(m_pArrayTexture_SRV)
				SAFE_RELEASE(unorderedAccessView);
			}
			void create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files,const std::vector<std::string> &pathsUtf8);
			void create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable);
			ID3D11Texture2D*					m_pArrayTexture;
			ID3D11ShaderResourceView*			m_pArrayTexture_SRV;
			ID3D11UnorderedAccessView*			unorderedAccessView;
		};
		inline void cancelStreamOutTarget(ID3D11DeviceContext *pContext)
		{
			ID3D11Buffer *pBuffer =NULL;
			UINT offset=0;
			pContext->SOSetTargets(1,&pBuffer,&offset);
		}
		inline void SetDebugObjectName( ID3D11DeviceChild* resource,const char *name)
		{
		  #if (defined(_DEBUG) || defined(PROFILE)) && !defined(_XBOX_ONE)
			if(resource)
				resource->SetPrivateData(WKPDID_D3DDebugObjectName,(UINT)(name?strlen(name):0),name?name:"un-named resource");
			#else
			name;resource;
		  #endif
		}
	}
}

#define SET_VERTEX_BUFFER(context,vertexBuffer,VertexType)\
	UINT stride = sizeof(VertexType);\
	UINT offset = 0;\
	context->IASetVertexBuffers(	0,				\
									1,				\
									&vertexBuffer,	\
									&stride,		\
									&offset);

#endif //SIMUL_PLATFORM_DIRECTX11_UTILITIES_H
