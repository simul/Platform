#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"

#if defined(_XBOX_ONE)
	#include <d3dx12_x.h>		
#else
	#include "d3dx12.h"
#endif

#include <vector>
#include <queue>


#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		class Material;
		class ConstantBufferBase;
	}
	namespace dx12
	{
		class Heap;
		class Fence;
		class ConstantBufferCache;
		class Material;
		//! A class to implement common rendering functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT RenderPlatform: public crossplatform::RenderPlatform
		{
		public:
										RenderPlatform();
			virtual						~RenderPlatform();
			const char*					GetName() const
			{
				return "DirectX 12";
			}

			//! Returns the current idx (used in ring buffers)
			UCHAR GetIdx()const { return mCurIdx; }

			//! Returns the time stamp freq value
			UINT64 GetTimeStampFreq()const { return mTimeStampFreq; }

			//! Sets the reference of a command list. This is usually not needed as we will cache
			//! the command list after calling render platform methods. We will need to call this
			//! during initialization (the command list hasn't been cached yet)
			void SetCommandList(ID3D12GraphicsCommandList* cmdList);

			//! Returns the command list reference
			ID3D12GraphicsCommandList*		AsD3D12CommandList();
			//! Returns the device provided during RestoreDeviceObjects
			ID3D12Device*					AsD3D12Device();
			//! Returns the queue provided during RestoreDeviceObjects (we only need a queue for fencing)
			ID3D12CommandQueue*				GetCommandQueue() { return m12Queue; }

			//! Method to transition a resource from one state to another. We can provide a subresource index
			//! to only update that subresource, leave as default if updating the hole resource
			void						ResourceTransitionSimple(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,UINT subRes = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			//! Pushes the pending barriers.
			void						FlushBarriers();
			
			//! Keeps track of a resource that must be released. It has a delay of a few frames
			//! so we the object won't be deleted while in use
			void						PushToReleaseManager(ID3D12DeviceChild* res, std::string dName);
			//! Clears the input assembler state (index and vertex buffers)
			void						ClearIA(crossplatform::DeviceContext &deviceContext);

			dx12::Heap*					SamplerHeap()		{return mSamplerHeap;}
			dx12::Heap*					RenderTargetHeap()	{ return mRenderTargetHeap; }
			dx12::Heap*					DepthStencilHeap()	{ return mDepthStencilHeap; }

			//! Returns the 2D dummy texture
			crossplatform::Texture*		GetDummy2D()		{ return mDummy2D;}
			//! Returns the 3D dummy texture
			crossplatform::Texture*		GetDummy3D()		{ return mDummy3D; }

			//! Returns the currently applied input layout
			D3D12_INPUT_LAYOUT_DESC*	GetCurrentInputLayout() { return mCurInputLayout; }
			//! Sets the current input layout
			void						SetCurrentInputLayout(D3D12_INPUT_LAYOUT_DESC* l) { mCurInputLayout = l; }
			//! Returns the current render target pixel format
			DXGI_FORMAT					GetCurrentPixelFormat() { return mCurPixelFormat; }
			//! Sets the current render target pixel format
			void						SetCurrentPixelFormat(DXGI_FORMAT format) { mCurPixelFormat = format; }
			//! Returns the current applied primitive topology
			D3D_PRIMITIVE_TOPOLOGY		GetCurrentPrimitiveTopology() { return mStoredTopology; }

			void						RestoreDeviceObjects(void* device);
			void						InvalidateDeviceObjects();
			void						RecompileShaders();

			// D3D11_MAP_FLAG				GetMapFlags() { return 0; }
			bool						UsesFastSemantics() const
			{
				return !can_save_and_restore;
			}

			virtual void							BeginEvent(crossplatform::DeviceContext &deviceContext,const char *name);
			virtual void							EndEvent(crossplatform::DeviceContext &deviceContext);
			void									StartRender(crossplatform::DeviceContext &deviceContext);
			void									EndRender(crossplatform::DeviceContext &deviceContext);
			void									IntializeLightingEnvironment(const float pAmbientLight[3]);
			void									CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s);

			void									DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			
			void									ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int index);
			
			void									Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert);
			void									DrawIndexed		(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0) override;
			void									DrawMarker		(crossplatform::DeviceContext &deviceContext,const double *matrix);
			void									DrawCrossHair	(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition);
			void									DrawCamera		(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll);
			void									DrawLineLoop	(crossplatform::DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void									DrawTexture		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend=false,float gamma=1.0f,bool debug=false) override;
			void									DrawQuad		(crossplatform::DeviceContext &deviceContext);

			void									DrawLines		(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false);
			void									Draw2dLines		(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip);
			void									DrawCube		(crossplatform::DeviceContext &deviceContext);
			void									ApplyDefaultMaterial();

			crossplatform::Material					*CreateMaterial();
			crossplatform::Mesh						*CreateMesh();
			crossplatform::Light					*CreateLight();
			crossplatform::Texture					*CreateTexture(const char *lFileNameUtf8 = nullptr);
			crossplatform::BaseFramebuffer			*CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState				*CreateSamplerState(crossplatform::SamplerStateDesc *d);
			crossplatform::Effect					*CreateEffect();
			crossplatform::PlatformConstantBuffer	*CreatePlatformConstantBuffer();
			crossplatform::PlatformStructuredBuffer	*CreatePlatformStructuredBuffer();
			crossplatform::Buffer					*CreateBuffer();
			crossplatform::Layout					*CreateLayout(int num_elements,const crossplatform::LayoutDesc *);			
			crossplatform::RenderState				*CreateRenderState(const crossplatform::RenderStateDesc &desc);
			crossplatform::Query					*CreateQuery(crossplatform::QueryType q) override;
			crossplatform::Shader					*CreateShader() override;

			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer *const*buffers,const crossplatform::Layout *layout,const int *vertexSteps=NULL);
			void									SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0);
			void									ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth);
			void									DeactivateRenderTargets(crossplatform::DeviceContext &deviceContext) override;
		
			crossplatform::Viewport					GetViewport(crossplatform::DeviceContext &deviceContext,int index);
			void									SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps);
			void									SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer);
			
			void									SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t) override;
			void									SetLayout(crossplatform::DeviceContext &deviceContext,crossplatform::Layout *l) override;
			void									EnsureEffectIsBuilt(const char *filename_utf8, const std::vector<crossplatform::EffectDefineOptions> &options) override;

			void									StoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									PushRenderTargets(crossplatform::DeviceContext &deviceContext);
			void									PopRenderTargets(crossplatform::DeviceContext &deviceContext);
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
			void									Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			bool									ApplyContextState(crossplatform::DeviceContext &deviceContext, bool error_checking = true) override;

			virtual void							ClearTexture(crossplatform::DeviceContext &deviceContext, crossplatform::Texture *texture, const vec4& colour) override;

			static									DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p);
			static									crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
			crossplatform::ShaderResourceType		FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t);
			static int								ByteSizeOfFormatElement(DXGI_FORMAT format);
			static bool								IsTypeless(DXGI_FORMAT fmt, bool partialTypeless);
			static DXGI_FORMAT						TypelessToSrvFormat(DXGI_FORMAT fmt);
			static D3D12_QUERY_TYPE					ToD3dQueryType(crossplatform::QueryType t);
			static D3D12_QUERY_HEAP_TYPE			ToD3D12QueryHeapType(crossplatform::QueryType t);
			//! Returns the subresource of the provided arguments. If mip or layer equal -1, it will be interpreted as 0.
			//! If both -1, the hole resource index will be returned
			static UINT								GetResourceIndex(int mip, int layer, int mips, int layers);

		protected:

			//! The GPU timestamp counter frequency (in ticks/second)
			UINT64 mTimeStampFreq;

			//! Last frame number
			long long					mLastFrame;
			//! Value used to determine the number of "x" that we will have, this is useful in dx12
			//! as many times we can not reuse the same resource as in the last frame so we need to have 
			//! a ring buffer.
			static const int			kNumIdx = 3;
			//! Value used to select the current heap, it will be looping around: [0,kNumIdx)
			UCHAR						mCurIdx;

			//! Reference to the DX12 device
			ID3D12Device*				m12Device;
			//! Reference to the command queue
			ID3D12CommandQueue*			m12Queue;
			//! Reference to a command list
			ID3D12GraphicsCommandList*	mCommandList;

			//! This heap will be bound to the pipeline and we will be copying descriptors to it. 
			//! The frame heap is used to store CBV SRV and UAV
			dx12::Heap*					mFrameHeap;
			//! This heap will be bound to the pipeline and we will be copying descriptors to it.
			//! The sampler heap is used to store SAMPLERS
			dx12::Heap*					mFrameSamplerHeap;

			dx12::Heap*					mSamplerHeap;
			dx12::Heap*					mRenderTargetHeap;
			dx12::Heap*					mDepthStencilHeap;
					  
			//! Dummy 2D texture
			crossplatform::Texture*		mDummy2D;
			//! Dummy 3D texture
			crossplatform::Texture*		mDummy3D;

			//! Current applied input layout
			D3D12_INPUT_LAYOUT_DESC*	mCurInputLayout;

			//! Current render target format
			DXGI_FORMAT					mCurPixelFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

			//! The primitive topology in use
			D3D_PRIMITIVE_TOPOLOGY		mStoredTopology;

			//! Holds resources to be deleted and its age
			std::vector<std::pair<int, std::pair<std::string, ID3D12DeviceChild*>>> mResourceBin;

			std::vector<D3D12_RESOURCE_BARRIER> mPendingBarriers;

			bool isInitialized = false;

			std::vector<struct RTState*>	storedRTStates;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif