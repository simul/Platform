#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Sl/CppSl.hs"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"
#include "Simul/Base/RuntimeError.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT SamplerState:public simul::crossplatform::SamplerState
		{
		public:
			ID3D11SamplerState *m_pd3D11SamplerState;
			SamplerState();
			virtual ~SamplerState();
			ID3D11SamplerState *asD3D11SamplerState()
			{
				return m_pd3D11SamplerState;
			}
		};
		class SIMUL_DIRECTX11_EXPORT Texture:public simul::crossplatform::Texture
		{
		public:
			Texture();
			~Texture();
			void InvalidateDeviceObjects();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			bool IsValid() const;
			virtual ID3D11Texture2D *AsD3D11Texture2D()
			{
				return (ID3D11Texture2D*)texture;
			}
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return shaderResourceView;
			}
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()
			{
				return unorderedAccessView;
			}
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView()
			{
				return depthStencilView;
			}
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView()
			{
				return renderTargetView;
			}
			GLuint AsGLuint()
			{
				return 0;
			}
			// Use this dx11::Texture as a wrapper for a texture and its corresponding SRV. Both pointers are needed.
			void InitFromExternalD3D11Texture2D(ID3D11Texture2D *t,ID3D11ShaderResourceView *srv);

			
			ID3D11UnorderedAccessView**  unorderedAccessViewMips;
			ID3D11Resource*				stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			DXGI_FORMAT format;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			virtual void ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1);
			virtual void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0);
			virtual void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void map(ID3D11DeviceContext *context);
			bool isMapped() const;
			void unmap();
			vec4 GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap);
			void activateRenderTarget(crossplatform::DeviceContext &deviceContext);
			void deactivateRenderTarget();
			virtual int GetLength() const
			{
				return length;
			}
			virtual int GetWidth() const
			{
				return width;
			}
			virtual int GetDimension() const
			{
				return dim;
			}
			int GetSampleCount() const;
		protected:
			ID3D11DeviceContext *last_context;
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*   shaderResourceView;
			ID3D11UnorderedAccessView*  unorderedAccessView;
			ID3D11DepthStencilView*		depthStencilView;
			ID3D11RenderTargetView*		renderTargetView;
			
			D3D11_VIEWPORT						m_OldViewports[16];
			unsigned							num_OldViewports;
			friend class CubemapFramebuffer;
		};
	}
}

namespace std
{
	template<> inline void swap(simul::dx11::Texture& _Left, simul::dx11::Texture& _Right)
	{
		SIMUL_BREAK("No more swapping of dx11::Texture's");
	}
}