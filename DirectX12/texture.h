#pragma once
#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "SimulDirectXHeader.h"
#include "Platform/Core/RuntimeError.h"
#include "Heap.h"
#include <string>
#include <unordered_map>

#pragma warning(disable:4251)
namespace DirectX
{
	struct TexMetadata;
	class ScratchImage;
	struct Image;
}
namespace platform
{
	namespace dx12
	{
		//! Sampler class for DirectX12
		class SIMUL_DIRECTX12_EXPORT SamplerState:public crossplatform::SamplerState
		{
		public:
			SamplerState(crossplatform::SamplerStateDesc *d);
			virtual ~SamplerState() override;
			
			void InvalidateDeviceObjects();
			inline D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12SamplerState()
			{
				return &mCpuHandle;
			}
			inline void SetDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE h)
			{
				mCpuHandle=h;
			}
        private:
            D3D12_CPU_DESCRIPTOR_HANDLE     mCpuHandle;
            crossplatform::SamplerStateDesc mCachedDesc;
		};

		//! Texture class for DirectX12, it implement the base Texture methods
		class SIMUL_DIRECTX12_EXPORT Texture:public crossplatform::Texture
		{
		public:
			Texture();
			virtual ~Texture() override;

			void SetName(const char *n) override;

			//! Cleans all the resources related with this object
			void							InvalidateDeviceObjects();
			//! Loads this texture from a file
			void							LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8, bool gen_mips);
			//! Loads this texture from multiple files
			void							LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files, bool gen_mips);
			bool							IsValid() const;
			
			void StoreExternalState(crossplatform::ResourceState);
			void RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext) override;

			ID3D12Resource*					AsD3D12Resource() override;
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView, bool setState = true, bool pixelShader = true);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12UnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12DepthStencilView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12RenderTargetView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView);

			bool							IsComputable() const override;
			bool							HasRenderTargets() const override;

			//! Initializes this texture from an external (already created texture)
			bool							InitFromExternalD3D12Texture2D(crossplatform::RenderPlatform* renderPlatform, ID3D12Resource* t, bool make_rt = false, bool setDepthStencil = false);
			bool							InitFromExternalTexture2D(crossplatform::RenderPlatform* renderPlatform, void* t, int w, int l, crossplatform::PixelFormat f, bool make_rt = false, bool setDepthStencil = false, int numOfSamples = 1) override;
			bool							InitFromExternalTexture3D(crossplatform::RenderPlatform* renderPlatform, void* t, bool make_uav = false) override;

			void							copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void							setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			bool							ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1,bool rendertargets=false);
			bool							ensureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *renderPlatform, int w, int l, int m,
																			crossplatform::PixelFormat f
																			, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
																			, bool computable = false, bool rendertarget = false, bool depthstencil = false,
																			int num_samples = 1, int aa_quality = 0, bool wrap = false, 
																			vec4 clear = vec4(0.5f,0.5f,0.2f,1.0f),float clearDepth = 1.0f,uint clearStencil = 0, bool shared = false
																			, crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED) override;
			bool							ensureVideoTexture(crossplatform::RenderPlatform* renderPlatform, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType) override;
			bool							ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int mips,crossplatform::PixelFormat f
												,std::shared_ptr<std::vector<std::vector<uint8_t>>> data
												,bool computable=false,bool rendertarget=false,bool cubemap=false,bool depthstencil=false
												,crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED) override;
			void							ensureTexture1DSizeAndFormat(ID3D12Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void							ClearColour(crossplatform::GraphicsDeviceContext &deviceContext, vec4 colourClear) override;
			void							ClearDepthStencil(crossplatform::GraphicsDeviceContext &deviceContext, float depthClear, int stencilClear) override;
			void							GenerateMips(crossplatform::GraphicsDeviceContext &deviceContext) override;
			bool							isMapped() const;
			void							unmap();
			vec4							GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap);
			void							activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::TextureView textureView = crossplatform::TextureView());
			void							deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext);

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
			D3D12_RESOURCE_STATES GetState()
			{
				return mResourceState;
			}

			int GetSampleCount()const;

			bool AreSubresourcesInSameState(const crossplatform::SubresourceRange& subresourceRange);

			//! Returns the current state of the resource or subresource if provided.
			D3D12_RESOURCE_STATES GetCurrentState(crossplatform::DeviceContext& deviceContext, const crossplatform::SubresourceRange& subresourceRange = {});
			//! Sets the state of the resource or subresource if provided.
			void SetLayout(crossplatform::DeviceContext& deviceContext, D3D12_RESOURCE_STATES state, const crossplatform::SubresourceRange& subresourceRange = {}, bool flush = false);
			
			void SwitchToContext(crossplatform::DeviceContext &deviceContext);
			DXGI_FORMAT	dxgi_format;
			// Need an active command list to finish loading a texture!
			void FinishLoading(crossplatform::DeviceContext &deviceContext) override;

			bool IsYUV() const override { return yuv; }

		protected:
			void InitFormats(crossplatform::PixelFormat f);
			bool											EnsureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *renderPlatform, int w, int l, int m,
																			crossplatform::PixelFormat f
																			, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
																			, bool computable = false, bool rendertarget = false, bool depthstencil = false, 
																			int num_samples = 1, int aa_quality = 0, bool wrap = false, 
																			vec4 clear = vec4(0.5f,0.5f,0.2f,1.0f),float clearDepth = 1.0f,uint clearStencil = 0
																			, bool shared = false,crossplatform::CompressionFormat cf=crossplatform::CompressionFormat::UNCOMPRESSED);
			void											FreeUAVTables();
			void											FreeSRVTables();
			void											FreeRTVTables();
			void											FreeDSVTables();

			void											InitStateTable(int l, int m);

			dx12::Heap						mTextureSrvHeap;
			dx12::Heap						mTextureUavHeap;
			dx12::Heap						mTextureRtHeap;
			dx12::Heap						mTextureDsHeap;

			//! Texture data that lives in the GPU
			ID3D12Resource*									mTextureDefault;
			//! Used to upload texture data to the GPU
			ID3D12Resource*									mTextureUpload;
			//! States of the subresources mSubResourcesStates[index][mip]
			std::vector<std::vector<D3D12_RESOURCE_STATES>>	mSubResourcesStates;
			//! Full resource state
			D3D12_RESOURCE_STATES			mResourceState;
			D3D12_RESOURCE_STATES			mExternalLayout;

			bool							mLoadedFromFile;	
			bool							mInitializedFromExternal = false;
			
			std::unordered_map<uint64_t, D3D12_CPU_DESCRIPTOR_HANDLE*> shaderResourceViews;
			std::unordered_map<uint64_t, D3D12_CPU_DESCRIPTOR_HANDLE*> unorderedAccessViews;
			std::unordered_map<uint64_t, D3D12_CPU_DESCRIPTOR_HANDLE*> depthStencilViews;
			std::unordered_map<uint64_t, D3D12_CPU_DESCRIPTOR_HANDLE*> renderTargetViews;

            //! We need to store the old MSAA state
            DXGI_SAMPLE_DESC                mCachedMSAAState;
            int                             mNumSamples;
			
			void AssumeLayout(D3D12_RESOURCE_STATES state);
			unsigned GetSubresourceIndex(int mip, int layer);
			
			struct WicContents
			{
				DirectX::TexMetadata	*metadata;
				DirectX::ScratchImage	*scratchImage;
				const DirectX::Image*	image;
			};
			std::vector<WicContents> wicContents;
			struct FileContents
			{
				void* ptr		= NULL;
				unsigned bytes	= 0;
				int flags		= 0;
			};
			std::vector<FileContents> fileContents;
			void ClearLoadingData();
			void ClearFileContents();
			void CreateUploadResource(int slices);
			std::vector <D3D12_PLACED_SUBRESOURCE_FOOTPRINT> pLayouts;
			DXGI_FORMAT genericDxgiFormat = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT srvFormat		= DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT uavFormat		=DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT Y_Format		=DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT UV_Format		=DXGI_FORMAT_UNKNOWN;
			bool yuv = false;
			bool textureLoadComplete = true;
			void FinishUploading(crossplatform::DeviceContext& deviceContext);
		};
	}
}
