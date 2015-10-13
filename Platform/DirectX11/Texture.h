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
		class SIMUL_DIRECTX11_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			ID3D11SamplerState *m_pd3D11SamplerState;
			SamplerState(crossplatform::SamplerStateDesc *d);
			virtual ~SamplerState() override;
			void InvalidateDeviceObjects();
			ID3D11SamplerState *asD3D11SamplerState()
			{
				return m_pd3D11SamplerState;
			}
		};
		class SIMUL_DIRECTX11_EXPORT Texture:public crossplatform::Texture
		{
		public:
			Texture();
			~Texture();
			void InvalidateDeviceObjects();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files);
			bool IsValid() const;
			ID3D11Texture2D *AsD3D11Texture2D()
			{
				return (ID3D11Texture2D*)texture;
			}
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView(int mip=-1)
			{
				if(mip<0||mips<=1)
					return shaderResourceView;
				if(mip<mips)
					return mipShaderResourceViews[mip];
				return NULL;
			}
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int mip=0)
			{
				if(mip<0||mip>=mips)
					return NULL;
				if(!unorderedAccessViews)
					return NULL;
				return unorderedAccessViews[mip];
			}
			ID3D11DepthStencilView *AsD3D11DepthStencilView()
			{
				return depthStencilView;
			}
			ID3D11RenderTargetView *AsD3D11RenderTargetView()
			{
				if(!renderTargetViews)
					return NULL;
				return *renderTargetViews;
			}
			GLuint AsGLuint()
			{
				return 0;
			}
			ID3D11RenderTargetView *ArrayD3D11RenderTargetView(int index)
			{
				if(!renderTargetViews||index<0||index>=num_rt)
					return NULL;
				return renderTargetViews[index];
			}
			// Use this dx11::Texture as a wrapper for a texture and its corresponding SRV. If a srv is not provided, one will be created internally. If \a make_rt is true and it is a rendertarget texture, a rendertarget will be created.
			void InitFromExternalD3D11Texture2D(crossplatform::RenderPlatform *renderPlatform,ID3D11Texture2D *t,ID3D11ShaderResourceView *srv,bool make_rt=false);
			void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false) override;
			ID3D11Resource				*stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			DXGI_FORMAT					format;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			void ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1,bool rendertargets=false);
			void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false
				,int num_samples=1,int aa_quality=0,bool wrap=false);
			void ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false) override;
			void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void GenerateMips(crossplatform::DeviceContext &deviceContext) override;
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
			ID3D11RenderTargetView*		m_pOldRenderTargets[16];
			ID3D11DepthStencilView*		m_pOldDepthSurface;
			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*   shaderResourceView;
			ID3D11UnorderedAccessView**  unorderedAccessViews;
			ID3D11ShaderResourceView**  mipShaderResourceViews;
			ID3D11DepthStencilView*		depthStencilView;
			ID3D11RenderTargetView**	renderTargetViews;
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