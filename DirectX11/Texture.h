#pragma once
#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include <string>
#include <unordered_map>
#include "DirectXHeader.h"
#include "Platform/Core/RuntimeError.h"

#pragma warning(disable:4251)

namespace platform
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
			void LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8, bool gen_mips) override;
			void LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files, bool gen_mips);
			bool IsValid() const;
			ID3D11Texture2D *AsD3D11Texture2D()
			{
				return (ID3D11Texture2D*)texture;
			}
			ID3D11Resource *AsD3D11Resource() override
			{
				return texture;
			}
			ID3D11ShaderResourceView* AsD3D11ShaderResourceView(const crossplatform::TextureView& textureView);
			ID3D11UnorderedAccessView* AsD3D11UnorderedAccessView(const crossplatform::TextureView& textureView);
			ID3D11DepthStencilView* AsD3D11DepthStencilView(const crossplatform::TextureView& textureView);
			ID3D11RenderTargetView* AsD3D11RenderTargetView(const crossplatform::TextureView& textureView);

			bool InitFromExternalTexture2D(crossplatform::RenderPlatform* renderPlatform, void* t, int w, int l, crossplatform::PixelFormat f, bool make_rt = false, bool setDepthStencil = false, int numOfSamples = 1) override;
			bool InitFromExternalTexture3D(crossplatform::RenderPlatform* renderPlatform, void* t, bool make_uav = false) override;
			ID3D11Resource				*stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			DXGI_FORMAT					dxgi_format;
			void copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			bool ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1,bool rendertargets=false);
			bool ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				, int m,crossplatform::PixelFormat f
				, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
				,bool computable=false,bool rendertarget=false,bool depthstencil=false
				,int num_samples=1,int aa_quality=0,bool wrap=false,
				vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0, bool shared = false
				, crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED) override;
			bool ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int mips
				,crossplatform::PixelFormat f
				, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
				,bool computable=false,bool rendertarget=false,bool cubemap=false,bool depthstencil=false
				, crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED) override;
			void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void ClearDepthStencil(crossplatform::GraphicsDeviceContext &deviceContext, float depthClear, int stencilClear) override;
			void GenerateMips(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void map(ID3D11DeviceContext *context);
			bool isMapped() const;
			void unmap();
			vec4 GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap);
			void activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::TextureView textureView = crossplatform::TextureView()) override;
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
			void FinishLoading(crossplatform::DeviceContext & );
			int GetSampleCount() const;
		protected:
			bool EnsureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform, int w, int l,int m
				, crossplatform::PixelFormat f
				, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
				, bool computable, bool rendertarget, bool depthstencil
				, int num_samples, int aa_quality, bool wrap,
				vec4 clear, float clearDepth, uint clearStencil
				, crossplatform::CompressionFormat compressionFormat);
			int GetMemorySize() const;
			ID3D11DeviceContext			*last_context;
			ID3D11Resource*				texture;
			ID3D11Resource				*external_copy_source;			// If this is a copy of an external texture, but that texture was stupidly not created to have SRV's,
																		// we must copy it for every update.
			std::unordered_map<uint64_t, ID3D11ShaderResourceView*> shaderResourceViews;
			std::unordered_map<uint64_t, ID3D11UnorderedAccessView*> unorderedAccessViews;
			std::unordered_map<uint64_t, ID3D11DepthStencilView*> depthStencilViews;
			std::unordered_map<uint64_t, ID3D11RenderTargetView*> renderTargetViews;
		
			void FreeUAVTables();
			void FreeSRVTables();
			void FreeRTVTables();
			void FreeDSVTables();
			DXGI_FORMAT genericDxgiFormat=DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT srvFormat=DXGI_FORMAT_UNKNOWN;
		};
	}
}
