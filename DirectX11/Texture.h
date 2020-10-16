#pragma once
#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/Shaders/Sl/CppSl.sl"
#include <string>
#include <map>
#include "DirectXHeader.h"
#include "Platform/Core/RuntimeError.h"

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
			virtual ~Texture() override;
			void InvalidateDeviceObjects();
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1);
			bool IsValid() const;
			ID3D11Texture2D *AsD3D11Texture2D()
			{
				return (ID3D11Texture2D*)texture;
			}
			ID3D11Resource *AsD3D11Resource() override
			{
				return texture;
			}
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView(crossplatform::ShaderResourceType t=crossplatform::ShaderResourceType::UNKNOWN,int index=-1,int mip=-1);
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int index=-1,int mip=-1);
			ID3D11DepthStencilView *AsD3D11DepthStencilView()
			{
				return depthStencilView;
			}
			ID3D11RenderTargetView *AsD3D11RenderTargetView(int index=-1,int mip=-1)
			{
				if(!renderTargetViews)
					return NULL;
				if(index<0)
					index=0;
				if(mip<0)
					mip=0;
#ifdef _DEBUG
				if(mip>=mips)
				{
					SIMUL_BREAK_ONCE("AsD3D11RenderTargetView: mip out of range");
					return NULL;
				}
				if(index>=NumFaces())
				{
					SIMUL_BREAK_ONCE("AsD3D11RenderTargetView: layer index out of range");
					return NULL;
				}
#endif
				return renderTargetViews[index][mip];
			}
			bool IsComputable() const override;
			bool HasRenderTargets() const override;
			void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt=false, bool setDepthStencil=false,bool need_srv=true, int numOfSamples = 1) override;
			void InitFromExternalTexture3D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_uav=false) override;
			ID3D11Resource				*stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			DXGI_FORMAT					dxgi_format;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			bool EnsureTexture(crossplatform::RenderPlatform *r,crossplatform::TextureCreate*) override;
			bool ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1,bool rendertargets=false);
			bool ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false
				,int num_samples=1,int aa_quality=0,bool wrap=false,
				vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0
			);
			bool ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int mips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false) override;
			void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void ClearDepthStencil(crossplatform::GraphicsDeviceContext &deviceContext, float depthClear, int stencilClear) override;
			void GenerateMips(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void map(ID3D11DeviceContext *context);
			bool isMapped() const;
			void unmap();
			vec4 GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap);
			void activateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext,int array_index=-1,int mip_index=0) override;
			void deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext) override;
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
			bool EnsureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform, int w, int l
				, crossplatform::PixelFormat f, bool computable, bool rendertarget, bool depthstencil
				, int num_samples, int aa_quality, bool wrap,
				vec4 clear, float clearDepth, uint clearStencil
				, crossplatform::CompressionFormat compressionFormat,const void *initData=nullptr);
			int GetMemorySize() const;
			ID3D11DeviceContext *last_context;
			ID3D11Resource*				texture;
			ID3D11Resource				*external_copy_source;			// If this is a copy of an external texture, but that texture was stupidly not created to have SRV's,
																		// we must copy it for every update.
			ID3D11ShaderResourceView*   mainShaderResourceView;			// SRV for the whole texture including all layers and mips.	
			ID3D11ShaderResourceView*	arrayShaderResourceView;		// SRV that describes a cubemap texture as an array, used only for cubemaps.
			ID3D11ShaderResourceView**	layerShaderResourceViews;		// SRV's for each layer, including all mips
			ID3D11ShaderResourceView**  mainMipShaderResourceViews;		// SRV's for the whole texture at different mips.
			ID3D11ShaderResourceView***	layerMipShaderResourceViews;	// SRV's for each layer at different mips.
			ID3D11UnorderedAccessView**  mipUnorderedAccessViews;		// UAV for the whole texture at various mips: only for 2D arrays.
			ID3D11UnorderedAccessView***  layerMipUnorderedAccessViews;	// UAV's for the layers and mips
			ID3D11DepthStencilView*		depthStencilView;
			ID3D11RenderTargetView***	renderTargetViews;				// 2D table: layers and mips.
		
			void InitUAVTables(int l,int m);
			void FreeUAVTables();
			void InitSRVTables(int l,int m);
			void FreeSRVTables();
			void FreeRTVTables();
			void InitRTVTables(int l,int m);
			void CreateSRVTables(int num,int m,bool cubemap,bool volume=false,bool msaa=false);
		};
	}
}
