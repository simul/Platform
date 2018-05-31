#pragma once
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Simul/Platform/DirectX12/ConstantBuffer.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"

#include "Heap.h"

#include <string>
#include <map>
#include <array>

#pragma warning(disable:4251)

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12ShaderReflection;
namespace simul
{
	namespace crossplatform
	{
		struct TextureAssignment;
	}
	namespace dx12
	{
		//! DirectX12 Query implementation
		struct SIMUL_DIRECTX12_EXPORT Query:public crossplatform::Query
		{
					Query(crossplatform::QueryType t);
			virtual ~Query() override;
			void	RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void	InvalidateDeviceObjects() override;
			void	Begin(crossplatform::DeviceContext &deviceContext) override;
			void	End(crossplatform::DeviceContext &deviceContext) override;
			bool	GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void	SetName(const char *n) override;

		protected:
			//! Holds the queries
			ID3D12QueryHeap*		mQueryHeap;
			//! We cache the query type
			D3D12_QUERY_TYPE		mD3DType;
			bool mIsDisjoint;
			//! Readback buffer to read data from the query
			ID3D12Resource*			mReadBuffer;
			//! We hold a pointer to the mapped data
			void*					mQueryData;
			UINT64					mTime;
		};

        struct D3D12_RENDER_TARGET_FORMAT_DESC
        {
            //! Returns an unique number that represents this rt desc
            inline size_t GetHash()
            {
                const size_t charSize = sizeof(char);
                size_t h      = 0;
                h            |= (size_t)RTFormats[0] << charSize * 8 * 0;
                h            |= (size_t)RTFormats[1] << charSize * 8 * 1;
                h            |= (size_t)RTFormats[2] << charSize * 8 * 2;
                h            |= (size_t)RTFormats[3] << charSize * 8 * 3;
                h            |= (size_t)RTFormats[4] << charSize * 8 * 4;
                h            |= (size_t)RTFormats[5] << charSize * 8 * 5;
                h            |= (size_t)RTFormats[6] << charSize * 8 * 6;
                h            |= (size_t)RTFormats[7] << charSize * 8 * 7;
                h            ^= (size_t)Count;
                return h;
            }
            UINT        Count;
            DXGI_FORMAT RTFormats[8];
        };

        //! Holds dx12 rendering state
		struct SIMUL_DIRECTX12_EXPORT RenderState:public crossplatform::RenderState
		{
			                                RenderState();
			virtual                         ~RenderState();
			D3D12_BLEND_DESC			    BlendDesc;
			D3D12_RASTERIZER_DESC		    RasterDesc;
			D3D12_DEPTH_STENCIL_DESC	    DepthStencilDesc;
            D3D12_RENDER_TARGET_FORMAT_DESC RtFormatDesc;
		};

		//! DirectX12 structured buffer class
		class EffectPass;
		class Effect;
		class RenderPlatform;
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
		public:
											PlatformStructuredBuffer();
			virtual							~PlatformStructuredBuffer();
			void							RestoreDeviceObjects(crossplatform::RenderPlatform* renderPlatform,int ct,int unit_size,bool computable,bool cpu_read,void *init_data);
			void							Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource &shaderResource);
            void                            ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource &shaderResource);
			//! Returns an initialized pointer with the size of this structured buffer that can be used to set data. After
			//! changing the data of this pointer, we must Apply this SB so the changes will be uploaded to the GPU.
			void*							GetBuffer(crossplatform::DeviceContext &deviceContext);
			//! Returns a pointer to the data of this structured buffer. This operation has a latency of
			//! mReadTotalCnt - 1 frames.
			const void*						OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			//! After opening the buffer, we close it.
			void							CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			//! Scheudles a copy so we can latter read the data.
			void							CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void							SetData(crossplatform::DeviceContext &deviceContext,void *data);
			void							InvalidateDeviceObjects();
			void							Unbind(crossplatform::DeviceContext &deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE*    AsD3D12ShaderResourceView(crossplatform::DeviceContext &deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE*    AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext,int = 0);
			void							ActualApply(simul::crossplatform::DeviceContext& deviceContext, EffectPass* currentEffectPass, int slot);

		private:
            //! If we called GetBuffer, we need to update the GPU data, this methods, handles updating the data at 
            //! different offsets + CPU<->GPU memory transition
            void                            UpdateBuffer(simul::crossplatform::DeviceContext& deviceContext);

            const D3D12_RESOURCE_STATES mShaderResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			static const unsigned int	mBuffering = 3;
			ID3D12Resource*				mGPUBuffer;
			ID3D12Resource*				mUploadBuffer;
			ID3D12Resource*			    mReadBuffers[mBuffering];

			//! We hold the currently mapped pointer
			UINT8*						mReadSrc;
			//! Temporal buffer used to upload data to the GPU
			void*						mTempBuffer;
			//! If true, we have to update the GPU data
			bool						mChanged;

			dx12::Heap					mBufferSrvHeap;
			dx12::Heap					mBufferUavHeap;

            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mSrvViews;
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mUavViews;

            // D3D12_CPU_DESCRIPTOR_HANDLE** mSrvViews;
            // D3D12_CPU_DESCRIPTOR_HANDLE** mUavViews;

			D3D12_RESOURCE_STATES		mCurrentState;

			//! Total number of individual elements
			int							mNumElements;
			//! Size of each element
			int							mElementByteSize;
			//! Total size (size of one unit * max applies)
			int							mTotalSize;
            int                         mUnitSize;
            //! Does this buffer allow reads from CPU?
            bool                        mCpuRead;

            //! How many times we can Apply this SB with different data
            //! During runtime we will check the current applies and recreate if needed
            int                         mMaxApplyMod = 1;
            int                         mCurApplies;
            uint64_t                    mLastFrame;
		};

        //! DirectX12 Effect Pass implementation, this will hold several PSOs, its also in charge of 
        // setting resources.
		class SIMUL_DIRECTX12_EXPORT EffectPass:public simul::crossplatform::EffectPass
		{
		public:
			            EffectPass();
			void        InvalidateDeviceObjects();
			void        Apply(crossplatform::DeviceContext &deviceContext,bool asCompute) override;
			bool        IsCompute()const { return mIsCompute; }

			void        SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers,dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void        SetConstantBuffers(crossplatform::ConstantBufferAssignmentMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device,crossplatform::DeviceContext& context);
			void        SetSRVs(crossplatform::TextureAssignmentMap &textures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void        SetUAVs(crossplatform::TextureAssignmentMap &rwTextures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
            
            void        CheckSlots(int requiredSlots, int usedSlots, int numSlots, const char* type);

            void        CreateComputePso(crossplatform::DeviceContext& deviceContext);
            size_t      CreateGraphicsPso(crossplatform::DeviceContext& deviceContext);
		private:
			virtual     ~EffectPass();
			//! We hold a map with unique PSOs
			std::unordered_map<size_t, ID3D12PipelineState*>                mGraphicsPsoMap;
            std::unordered_map<size_t, D3D12_RENDER_TARGET_FORMAT_DESC*>    mTargetsMap;
			//! We only have one compute Pipeline  
			ID3D12PipelineState*						mComputePso = nullptr;
			//! Is this a compute pass?
			bool                                        mIsCompute = false;
			std::vector<CD3DX12_DESCRIPTOR_RANGE>	    mSrvCbvUavRanges;
			std::vector<CD3DX12_DESCRIPTOR_RANGE>	    mSamplerRanges;
			std::string								    mTechName;
			//! Arrays used by the Set* methods declared here to avoid runtime memory allocation
			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumCBV>	mCbSrcHandles;

			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSRV>	mSrvSrcHandles;
			std::array<bool, ResourceBindingLimits::NumSRV>							mSrvUsedSlotsArray;

			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumUAV>	mUavSrcHandles;
			std::array<bool, ResourceBindingLimits::NumUAV>							mUavUsedSlotsArray;

            D3D12_DEPTH_STENCIL_DESC*   mInUseOverrideDepthState;
            D3D12_BLEND_DESC*           mInUseOverrideBlendState;
		};

		class SIMUL_DIRECTX12_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
		public:
			int NumPasses() const;
			crossplatform::EffectPass *AddPass(const char *name,int i) override;
		};

		class SIMUL_DIRECTX12_EXPORT Shader :public simul::crossplatform::Shader
		{
		public:
			void load(crossplatform::RenderPlatform *renderPlatform, const char *filename, crossplatform::ShaderType t) override;
			union
			{
				ID3DBlob*					vertexShader12;
				ID3DBlob*					pixelShader12;
				ID3DBlob*					computeShader12;
			};
			ID3D12ShaderReflection*			mShaderReflection = nullptr;
		};

        //! DirectX12 Effect implementation
		class SIMUL_DIRECTX12_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
		public:
			Effect();
			virtual ~Effect();

			void EnsureEffect(crossplatform::RenderPlatform *r, const char *filename_utf8);
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void PostLoad() override;
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique* GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index);

			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass) override;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass) override;
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext) override;
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
			void SetConstantBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::ConstantBufferBase *s) override;

			//! This method uses the shader reflection code to check the resources slots. This is needed because
			//! the sfx compiler is not as smart as the dx compiler so it will report that some resources are in use
			//! when it is not true. In Dx12 we MUST be explicit about what are we doing so we need to know exactly the
			//! resources that will be in use.
			void CheckShaderSlots(dx12::Shader* shader, ID3DBlob* shaderBlob);

            Heap* GetEffectSamplerHeap();

        private:
            //! Heap with the static samplers used by the effect
            Heap* mSamplersHeap;
		};
	}
}
