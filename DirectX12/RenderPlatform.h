#pragma once

#include "Export.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/BaseRenderer.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/DirectX12/GpuProfiler.h"
#include "Platform/CrossPlatform/Shaders/solid_constants.sl"
#include "Platform/CrossPlatform/Shaders/debug_constants.sl"
#include "SimulDirectXHeader.h"
#include "ThisPlatform/Direct3D12.h"
#include <vector>
#include <queue>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

extern const char *PlatformD3D12GetErrorText(HRESULT hr);
//#define VERIFY_EXPLICIT_CAST(from, to) static_assert(sizeof(from) == sizeof(to)) 
#ifndef V_CHECK
	#define V_CHECK(x)\
	{\
		HRESULT hrx ;\
		hrx = x;\
		if( FAILED(hrx) )\
		{\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: V_CHECK error, return value is "<<PlatformD3D12GetErrorText(hrx)<<std::endl;\
			BREAK_IF_DEBUGGING;\
		}\
	}
#endif
namespace platform
{
	//! Used to hold information about the resource binding limits and
	//! the current hardware binding tier
	struct ResourceBindingLimits
	{
		ResourceBindingLimits():
			BindingTier(D3D12_RESOURCE_BINDING_TIER_1),
			MaxShaderVisibleDescriptors(0),
			MaxCBVPerStage(0),
			MaxSRVPerStage(0),
			MaxUAVPerStage(0),
			MaxSamplersPerStage(0)
		{
		}
		D3D12_RESOURCE_BINDING_TIER BindingTier;

		//! Maximum possible number of descriptors (defined by the 
		//! hardware we are runing on)
		int							MaxShaderVisibleDescriptors;
		int							MaxCBVPerStage;
		int							MaxSRVPerStage;
		int							MaxUAVPerStage;
		int							MaxSamplersPerStage;

		static const int NumCBV		 = 14;
		static const int NumSRV		 = 32;
		static const int NumUAV		 = 16;
		static const int NumSamplers	= 16;
	};

	namespace crossplatform
	{
		class Material;
		class ConstantBufferBase;
	}
	namespace dx12
	{
		struct ImmediateContext
		{
			ID3D12GraphicsCommandListType*	ICommandList	=nullptr;
			ID3D12CommandAllocator*			IAllocator		=nullptr;
			bool							isExternal		=false;
		};
		struct D3D12ComputeContext 
		{
			void RestoreDeviceObjects(ID3D12Device* mDevice);
			void InvalidateDeviceObjects();

			ID3D12GraphicsCommandListType*	ICommandList	=nullptr;
			ID3D12CommandAllocator*			IAllocator		=nullptr;
		};
		class Heap;
		class Material;
		struct SIMUL_DIRECTX12_EXPORT Fence: public crossplatform::Fence
		{
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;
			Fence(crossplatform::RenderPlatform* r);
			~Fence();
			ID3D12Fence* AsD3D12Fence()
			{
				return d3d12Fence;
			}
		protected:
			ID3D12Fence *d3d12Fence=nullptr;
		};
		//! A class to implement common rendering functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT RenderPlatform : public crossplatform::RenderPlatform
		{
		public:
											RenderPlatform();
			virtual							~RenderPlatform();
			virtual float					GetDefaultOutputGamma() const override;
			const char* GetName() const override { return PLATFORM_NAME; }
			crossplatform::RenderPlatformType GetType() const override
			{
#if defined(_GAMING_XBOX_SCARLETT)
				return crossplatform::RenderPlatformType::Spectrum;
#elif defined(_GAMING_XBOX_XBOXONE)
				return crossplatform::RenderPlatformType::XboxOneD3D12;
#else
				return crossplatform::RenderPlatformType::D3D12;
#endif
			}
			virtual const char *			GetSfxConfigFilename() const override
			{
				return SFX_CONFIG_FILENAME;
			}
			//! Returns the time stamp freq value
			UINT64							GetTimeStampFreq() const		 { return mTimeStampFreq; }
			//! Sets the reference of a command list. This is usually not needed as we will cache
			//! the command list after calling render platform methods. We will need to call this
			//! during initialization (the command list hasn't been cached yet)
			void							SetImmediateContext(ImmediateContext* ctx);

			//! Returns the device provided during RestoreDeviceObjects
			ID3D12Device*					AsD3D12Device();
			//! Returns the device for raytracing, or nullptr if unavailable. You must call Release() on this pointer, as it is created via QueryInterface().
#if !defined(_GAMING_XBOX_XBOXONE)
			ID3D12Device5*					AsD3D12Device5();
#endif
			//! Returns the queue created during RestoreDeviceObjects
			ID3D12CommandQueue* GetCommandQueue(crossplatform::DeviceContextType type = crossplatform::DeviceContextType::GRAPHICS) 
			{ 
				switch (type)
				{
				default:
				case platform::crossplatform::DeviceContextType::GRAPHICS:
					return mGraphicsQueue;
				case platform::crossplatform::DeviceContextType::COMPUTE:
					return mComputeQueue;
				case platform::crossplatform::DeviceContextType::COPY:
					return mCopyQueue;
				}
			}
			//! Method to transition a resource from one state to another. We can provide a subresource index
			//! to only update that subresource, leave as default if updating the hole resource. Transitions will be stored
			//! and executed all at once before important calls. Set flush to true to perform the action immediatly
			void							ResourceTransitionSimple(crossplatform::DeviceContext& deviceContext,ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, bool flush = false, UINT subRes = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			//! Pushes the pending barriers.
			void							FlushBarriers(crossplatform::DeviceContext& deviceContext);
			//! Keeps track of a resource that must be released. It has a delay of a few frames
			//! so we the object won't be deleted while in use
			void							PushToReleaseManager(ID3D12Object* res, AllocationInfo *allocationInfo = nullptr, bool owned = true);
			void							ClearReleaseManager(bool force = false);
			//! Clears the input assembler state (index and vertex buffers)
			void							ClearIA(crossplatform::DeviceContext &deviceContext);

			dx12::Heap*						SamplerHeap()				{ return mSamplerHeap;}
			dx12::Heap*						RenderTargetHeap()			{ return mRenderTargetHeap; }
			dx12::Heap*						DepthStencilHeap()			{ return mDepthStencilHeap; }

			//! Returns the 2D dummy texture
			crossplatform::Texture*			GetDummy2D()const			{ return mDummy2D;}
			//! Returns the 3D dummy texture
			crossplatform::Texture*			GetDummy3D()const			{ return mDummy3D; }

			//! Returns the currently applied input layout
			D3D12_INPUT_LAYOUT_DESC*		GetCurrentInputLayout()	 { return mCurInputLayout; }
			//! Sets the current input layout
			void							SetCurrentInputLayout(D3D12_INPUT_LAYOUT_DESC* l) { mCurInputLayout = l; }
			//! Returns the current applied primitive topology
			D3D_PRIMITIVE_TOPOLOGY			GetCurrentPrimitiveTopology() { return mStoredTopology; }

			//! Platform-dependent function called when initializing the render platform.
			void							RestoreDeviceObjects(void* device);
			//! Platform-dependent function called when uninitializing the render platform.
			void							InvalidateDeviceObjects();
			void							SynchronizeCacheAndState(crossplatform::DeviceContext &) override;

			crossplatform::GraphicsDeviceContext& GetImmediateContext() override;

			virtual void							BeginEvent(crossplatform::DeviceContext &deviceContext,const char *name);
			virtual void							EndEvent(crossplatform::DeviceContext &deviceContext);

			void									BeginFrame() override;
			void									EndFrame() override;
			void									ResourceTransition(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex, crossplatform::ResourceTransition transition)override;
			void									ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)override;
			void									ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb) override;
			void									CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *dst,crossplatform::Texture *src);
			void									DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d) override;
			void									DispatchRays	(crossplatform::DeviceContext &deviceContext, const uint3 &dispatch, const crossplatform::ShaderBindingTable* sbt = nullptr) override;
			void									Signal			(crossplatform::DeviceContextType& type, crossplatform::Fence::Signaller signaller, crossplatform::Fence* fence) override;
			void									Wait			(crossplatform::DeviceContextType& type, crossplatform::Fence::Waiter waiter, crossplatform::Fence* fence, uint64_t timeout_nanoseconds = UINT64_MAX) override;
			bool									GetFenceStatus	(crossplatform::Fence* fence) override;
			void									ExecuteCommands (crossplatform::DeviceContext& deviceContext) override;
			void									RestartCommands (crossplatform::DeviceContext& deviceContext) override;
			void									Draw			(crossplatform::GraphicsDeviceContext &GraphicsDeviceContext,int num_verts,int start_vert);
			void									DrawIndexed		(crossplatform::GraphicsDeviceContext &GraphicsDeviceContext,int num_indices,int start_index=0,int base_vertex=0) override;
			
			void									DrawQuad		(crossplatform::GraphicsDeviceContext &GraphicsDeviceContext);

			void									ApplyDefaultMaterial();

			crossplatform::Framebuffer				*CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState				*CreateSamplerState(crossplatform::SamplerStateDesc *d) override;
			crossplatform::Effect					*CreateEffect() override;
			crossplatform::PlatformConstantBuffer	*CreatePlatformConstantBuffer(crossplatform::ResourceUsageFrequency F) override;
			crossplatform::PlatformStructuredBuffer	*CreatePlatformStructuredBuffer() override;
			crossplatform::Buffer					*CreateBuffer() override;
			crossplatform::Layout					*CreateLayout(int num_elements,const crossplatform::LayoutDesc *,bool) override;
			crossplatform::RenderState				*CreateRenderState(const crossplatform::RenderStateDesc &desc);
			crossplatform::Query					*CreateQuery(crossplatform::QueryType q) override;
			crossplatform::Fence					*CreateFence(const char* name) override;
			crossplatform::Shader					*CreateShader() override;
			crossplatform::BottomLevelAccelerationStructure*CreateBottomLevelAccelerationStructure() override;
			crossplatform::TopLevelAccelerationStructure*CreateTopLevelAccelerationStructure() override;
			crossplatform::ShaderBindingTable*		CreateShaderBindingTable() override;
			crossplatform::DisplaySurface*			CreateDisplaySurface() override;
			crossplatform::GpuProfiler*				CreateGpuProfiler() override;
			
			void									PrintDeviceRemovedExtendedData();

			void									PresentSwapChain(crossplatform::DeviceContext &, crossplatform::Window *s) {};

			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext, int slot, int num_buffers, const crossplatform::Buffer* const* buffers, const crossplatform::Layout* layout, const int* vertexSteps = NULL)override;
			void									ClearVertexBuffers(crossplatform::DeviceContext& deviceContext) override;
			void									SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0);

			void									ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth);
			void									ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext, crossplatform::TargetsAndViewport* targets)override;
			
			void									DeactivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext) override;
			void									ApplyDefaultRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext) override;
		
			void									SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps);
			void									SetIndexBuffer(crossplatform::GraphicsDeviceContext &deviceContext, const crossplatform::Buffer *buffer);
			
			void									SetTopology(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Topology t) override;
			void									SetLayout(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Layout *l) override;

			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
			void									Resolve(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::GraphicsDeviceContext &graphicsDeviceContext,crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			bool									ApplyContextState(crossplatform::DeviceContext &deviceContext, bool error_checking = true) override;

			static									DXGI_FORMAT ToDxgiFormat(crossplatform::PixelOutputFormat p);
			static									DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p,crossplatform::CompressionFormat c);
			static									crossplatform::CompressionFormat DxgiFormatToCompressionFormat(DXGI_FORMAT dxgi_format);
			static									crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
			crossplatform::ShaderResourceType		FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t);
			static int								ByteSizeOfFormatElement(DXGI_FORMAT format);
			static bool								IsTypeless(DXGI_FORMAT fmt, bool partialTypeless);
			static DXGI_FORMAT						TypelessToSrvFormat(DXGI_FORMAT fmt);
			static DXGI_FORMAT						TypelessToDsvFormat(DXGI_FORMAT fmt);
			static DXGI_FORMAT						DsvToTypelessFormat(DXGI_FORMAT fmt);
			static void								DepthFormatToResourceAndSrvFormats(DXGI_FORMAT& texture2dFormat, DXGI_FORMAT& srvFormat);
			static D3D12_QUERY_TYPE					ToD3dQueryType(crossplatform::QueryType t);
			static D3D12_QUERY_HEAP_TYPE			ToD3D12QueryHeapType(crossplatform::QueryType t);
			static std::string						D3D12ResourceStateToString(D3D12_RESOURCE_STATES states);
			//! We cache the current number of samples
			void									SetCurrentSamples(int samples, int quality = 0);
			bool									IsMSAAEnabled();

			//! Executes the provided commandList on the provided commandQueue, alongwith executing the internal immediate commandlist on the graphics queue.
			//! Passing nullptr for both parameters will only execute the immediate commandlist.
			void ExecuteCommandList(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* const commandList);
		

			DXGI_SAMPLE_DESC						GetMSAAInfo();
			
			ResourceBindingLimits					GetResourceBindingLimits()const;
			ID3D12RootSignature*					GetGraphicsRootSignature()const;
			ID3D12RootSignature*					GetRaytracingLocalRootSignature() const;
			ID3D12RootSignature*					GetRaytracingGlobalRootSignature() const;
			
			D3D12_CPU_DESCRIPTOR_HANDLE				GetNullCBV()const;
			D3D12_CPU_DESCRIPTOR_HANDLE				GetNullSRV()const;
			D3D12_CPU_DESCRIPTOR_HANDLE				GetNullSBSRV()const;
			D3D12_CPU_DESCRIPTOR_HANDLE				GetNullUAV()const;
			D3D12_CPU_DESCRIPTOR_HANDLE				GetNullSampler()const;

			//! Holds, if any, an override depth state
			D3D12_DEPTH_STENCIL_DESC*				DepthStateOverride;
			//! Holds, if any, an override blend state
			D3D12_BLEND_DESC*						BlendStateOverride;
			//! Holds, if any, an override raster state
			D3D12_RASTERIZER_DESC*					RasterStateOverride;

			D3D12_DEPTH_STENCIL_DESC				DefaultDepthState;
			D3D12_BLEND_DESC						DefaultBlendState;
			D3D12_RASTERIZER_DESC					DefaultRasterState;

			crossplatform::PixelFormat				DefaultOutputFormat;

			void									CreateResource(const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES state, 
																	D3D12_CLEAR_VALUE *clear, D3D12_HEAP_TYPE type, ID3D12Resource **resource, 
																	AllocationInfo &allocationInfo, const char *name);
			
		protected:
			D3D12MA::Allocator *mCPUAllocator = nullptr;
			D3D12MA::Allocator *mGPUAllocator = nullptr;
			UINT mCPUPreferredBlockSize = 0;
			UINT mGPUPreferredBlockSize = 0;

			friend class CommandListController;
			static ID3D12CommandQueue* CreateCommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, const char* name);
			static void FlushCommandQueue(ID3D12Device* device, ID3D12CommandQueue* queue);
			//D3D12-specific things
			virtual void ContextFrameBegin(crossplatform::GraphicsDeviceContext&) override;
			crossplatform::Texture* createTexture() override;
			void						CheckBarriersForResize(crossplatform::DeviceContext &deviceContext);
			//! The GPU timestamp counter frequency (in ticks/second)
			UINT64						mTimeStampFreq;
			//! Reference to the DX12 device
			ID3D12Device*				m12Device=nullptr;
			//! Reference to the graphics command queue
			ID3D12CommandQueue*			mGraphicsQueue=nullptr;
			//! Reference to the compute command queue
			ID3D12CommandQueue*			mComputeQueue=nullptr;
			//! Reference to the copy command queue
			ID3D12CommandQueue*			mCopyQueue=nullptr;
			// Immediate context can be owned or external.
			ImmediateContext			mIContext;
			//! This heap will be bound to the pipeline and we will be copying descriptors to it. 
			//! The frame heap is used to store CBV SRV and UAV
			dx12::Heap*					mFrameHeap=nullptr;
			//! Heap used to hold override sampler states
			dx12::Heap*					mFrameOverrideSamplerHeap=nullptr;
			dx12::Heap*					mSamplerHeap=nullptr;
			dx12::Heap*					mRenderTargetHeap=nullptr;
			dx12::Heap*					mDepthStencilHeap=nullptr;
			dx12::Heap*					mNullHeap=nullptr;
			//! Shared root signature for graphics
			ID3D12RootSignature*		mGRootSignature=nullptr;
			//! For raytracing
			ID3D12RootSignature*		mGRaytracingLocalSignature=nullptr;
			ID3D12RootSignature*		mGRaytracingGlobalSignature=nullptr;

			//! Dummy 2D texture
			crossplatform::Texture*		mDummy2D;
			//! Dummy 3D texture
			crossplatform::Texture*		mDummy3D;
			//! Current applied input layout
			D3D12_INPUT_LAYOUT_DESC*	mCurInputLayout;
			//! The primitive topology in use
			D3D_PRIMITIVE_TOPOLOGY		mStoredTopology;

			//! Holds resources to be deleted and its age
			struct ReleaseResourceInfo
			{
				ID3D12Object *resource = nullptr;
				AllocationInfo allocationInfo;
				unsigned int age = 0;
				bool owned = true;
			};
			std::vector<ReleaseResourceInfo> mReleaseResources;

			//! Default number of barriers we hold, the number will increase
			//! if we run out of barriers
			struct ContextBarriers
			{
				ContextBarriers()
				{
					mCurBarriers    = 0;
					mTotalBarriers  = 16;
					mPendingBarriers.resize(mTotalBarriers);
				}

				int									mTotalBarriers;
				int									mCurBarriers;
				std::vector<D3D12_RESOURCE_BARRIER> mPendingBarriers;
			};
			std::map<ID3D12CommandList*,ContextBarriers*> barriers;
			
			ContextBarriers &GetBarriers(crossplatform::DeviceContext &);
			bool								isInitialized = false;
			bool								mIsMsaaEnabled;
			DXGI_SAMPLE_DESC					mMsaaInfo;			

			ResourceBindingLimits				mResourceBindingLimits;
			D3D12_CPU_DESCRIPTOR_HANDLE			mNullCBV;
			D3D12_CPU_DESCRIPTOR_HANDLE			mNullSRV;
			D3D12_CPU_DESCRIPTOR_HANDLE			mNullSBSRV;
			D3D12_CPU_DESCRIPTOR_HANDLE			mNullUAV;
			D3D12_CPU_DESCRIPTOR_HANDLE			mNullSampler;

			crossplatform::TargetsAndViewport mTargets;
			#if !defined(_GAMING_XBOX)
			ID3D12DeviceRemovedExtendedDataSettings * pDredSettings=nullptr;
			#endif
			ID3D12RootSignature *LoadRootSignature(const char *filename);

			D3D12ComputeContext m12ComputeContext;
			std::deque<std::pair<crossplatform::Fence*, ID3D12CommandAllocator*>> mUsedAllocators;
		public:
			void FlushImmediateCommandList();
			void ResetImmediateCommandList() override;
			unsigned char frameHeapIndex=0;
		};

	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif