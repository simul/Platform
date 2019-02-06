#pragma once
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Sl/CppSl.hs"
#include "SimulDirectXHeader.h"
#include "Simul/Base/RuntimeError.h"
#include "Heap.h"
#include <string>

#pragma warning(disable:4251)
namespace DirectX
{
	struct TexMetadata;
	struct ScratchImage;
}
namespace simul
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
			void							LoadFromFile(crossplatform::RenderPlatform *r,const char *pFilePathUtf8);
			//! Loads this texture from multiple files
			void							LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1);
			bool							IsValid() const;
			
			ID3D12Resource*					AsD3D12Resource() override;
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12ShaderResourceView(bool setState = true,crossplatform::ShaderResourceType t= crossplatform::ShaderResourceType::UNKNOWN, int = -1, int = -1);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12UnorderedAccessView(int index = -1, int mip = -1);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12DepthStencilView();
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12RenderTargetView(int index = -1, int mip = -1);

			bool							IsComputable() const override;
			bool							HasRenderTargets() const override;

			//! Initializes this texture from an external (already created texture)
			void							InitFromExternalD3D12Texture2D(crossplatform::RenderPlatform *renderPlatform, ID3D12Resource* t, D3D12_CPU_DESCRIPTOR_HANDLE* srv, bool make_rt = false, bool setDepthStencil = false,bool need_srv=true);
			void							InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt=false, bool setDepthStencil=false,bool need_srv=true, int numOfSamples = 1) override;
			void							InitFromExternalTexture3D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_uav=false) override;

			void							copyToMemory(crossplatform::DeviceContext &deviceContext,void *target,int start_texel=0,int texels=0);
			void							setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels);
			bool							ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int d,crossplatform::PixelFormat f,bool computable,int mips=1,bool rendertargets=false);
			bool							ensureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *renderPlatform, int w, int l, 
																			crossplatform::PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, 
																			int num_samples = 1, int aa_quality = 0, bool wrap = false, 
																			vec4 clear = vec4(0.5f,0.5f,0.2f,1.0f),float clearDepth = 1.0f,uint clearStencil = 0);
			bool							ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num,int mips,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false) override;
			void							ensureTexture1DSizeAndFormat(ID3D12Device *pd3dDevice,int w,crossplatform::PixelFormat f,bool computable=false);
			void							ClearDepthStencil(crossplatform::DeviceContext &deviceContext, float depthClear, int stencilClear) override;
			void							GenerateMips(crossplatform::DeviceContext &deviceContext) override;
			bool							isMapped() const;
			void							unmap();
			vec4							GetTexel(crossplatform::DeviceContext &deviceContext,vec2 texCoords,bool wrap);
			void							activateRenderTarget(crossplatform::DeviceContext &deviceContext,int array_index=-1,int mip_index=0);
			void							deactivateRenderTarget(crossplatform::DeviceContext &deviceContext);

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
			int GetSampleCount()const;

			//! Returns the current state of the resource or subresource if provided.
			D3D12_RESOURCE_STATES GetCurrentState(int mip = -1, int index = -1);
			//! Sets the state of the resource or subresource if provided.
			void SetCurrentState(D3D12_RESOURCE_STATES state, int mip = -1, int index = -1);

			DXGI_FORMAT	dxgi_format;
			// Need an active command list to finish loading a texture!
			void FinishLoading(crossplatform::DeviceContext &deviceContext) override;

		protected:
			void											InitUAVTables(int l, int m);
			void											FreeUAVTables();

			void											InitSRVTables(int l, int m);
			void											FreeSRVTables();
			void											CreateSRVTables(int num, int m, bool cubemap, bool volume = false, bool msaa = false);

			void											FreeRTVTables();
			void											InitRTVTables(int l, int m);

			void											InitStateTable(int l, int m);

			//! Texture data that lives in the GPU
			ID3D12Resource*									mTextureDefault;
			//! Used to upload texture data to the GPU
			ID3D12Resource*									mTextureUpload;
			//! States of the subresources mSubResourcesStates[index][mip]
			std::vector<std::vector<D3D12_RESOURCE_STATES>>	mSubResourcesStates;
			//! Full resource state
			D3D12_RESOURCE_STATES							mResourceState;

			dx12::Heap						mTextureSrvHeap;
			dx12::Heap						mTextureUavHeap;
			dx12::Heap						mTextureRtHeap;
			dx12::Heap						mTextureDsHeap;

			bool							mLoadedFromFile;	
			bool							mInitializedFromExternal = false;
			
			D3D12_CPU_DESCRIPTOR_HANDLE		mainShaderResourceView12;		// SRV for the whole texture including all layers and mips.	

			D3D12_CPU_DESCRIPTOR_HANDLE		arrayShaderResourceView12;		// SRV that describes a cubemap texture as an array, used only for cubemaps.

			D3D12_CPU_DESCRIPTOR_HANDLE*	layerShaderResourceViews12;		// SRV's for each layer, including all mips
			D3D12_CPU_DESCRIPTOR_HANDLE*	mainMipShaderResourceViews12;	// SRV's for the whole texture at different mips.
			D3D12_CPU_DESCRIPTOR_HANDLE**	layerMipShaderResourceViews12;	// SRV's for each layer at different mips.

			D3D12_CPU_DESCRIPTOR_HANDLE*	mipUnorderedAccessViews12;		// UAV for the whole texture at various mips: only for 2D arrays.
			D3D12_CPU_DESCRIPTOR_HANDLE**	layerMipUnorderedAccessViews12;	// UAV's for the layers and mips

			D3D12_CPU_DESCRIPTOR_HANDLE		depthStencilView12;
			D3D12_CPU_DESCRIPTOR_HANDLE**	renderTargetViews12;			// 2D table: layers and mips.

            //! We need to store the old MSAA state
            DXGI_SAMPLE_DESC                mCachedMSAAState;
            int                             mNumSamples;
			DirectX::TexMetadata	*metadata;
			DirectX::ScratchImage	*scratchImage;
			void *loadedData;
		};
	}
}
