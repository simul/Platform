#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"

#include "Heap.h"
#include "Fence.h"

#include "SimulDirectXHeader.h"
#include <vector>
#include <queue>

#include "d3d12.h"
#include "d3dx12.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
#ifndef _XBOX_ONE
	struct ID3DUserDefinedAnnotation;
#endif


namespace simul
{
	namespace crossplatform
	{
		class Material;
		class ConstantBufferBase;
	}
	namespace dx11on12
	{
		class ConstantBufferCache;
		class Material;
		//! A class to implement common rendering functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
			/// Last frame number
			UINT64						mLastFrame = UINTMAX_MAX;
			/// Reference to the graphical command list
			ID3D12GraphicsCommandList*  m12CommandList;
			/// Reference to the DX12 device
			ID3D12Device*				m12Device;
			/// Reference to the command queue
			ID3D12CommandQueue*			m12Queue;

			/// This heaps will be bound to the command list, we will be building it with copies
			/// from the required resources
			dx12::Heap					mFrameHeap[3];
			dx12::Heap					mFrameSamplerHeap[3];

			dx12::Heap					mSamplerHeap;
			dx12::Heap					mRenderTargetHeap;
			dx12::Heap					mDepthStencilHeap;

			/// Dummy resources
			crossplatform::Texture*		mDummy2D;
			crossplatform::Texture*		mDummy3D;

			dx12::Fence					mComputeFence;

			/// Current applied input layout
			D3D12_INPUT_LAYOUT_DESC*	mCurInputLayout;

			/// Current render target format
			DXGI_FORMAT					mCurPixelFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

			/// The primitive topology in use
			D3D_PRIMITIVE_TOPOLOGY		mStoredTopology;

			/// Holds resources to be deleted and its age
			std::vector<std::pair<int, std::pair<std::string,ID3D12Resource*>>>			mResourceBin;
			std::vector<std::pair<int, std::pair<std::string, ID3D12DescriptorHeap*>>>	mHeapBin;

		public:
										RenderPlatform();
			virtual						~RenderPlatform();
			const char*					GetName() const
			{
				return "DirectX 12";
			}
			
			ID3D12GraphicsCommandList*	AsD3D12CommandList();
			ID3D12Device*				AsD3D12Device();
			ID3D12CommandQueue*			GetCommandQueue() { return m12Queue; }

			/// Method to transition a resource from one state to another
			void						ResourceTransitionSimple(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
			/// Keeps track of a resource that must be release at some point
			void						PushToReleaseManager(ID3D12Resource* res,std::string dName);
			void						PushToReleaseManager(ID3D12DescriptorHeap* res, std::string dName);
			/// Clears the input assembler state (index and vertex buffers)
			void						ClearIA(crossplatform::DeviceContext &deviceContext);

			dx12::Heap*					SamplerHeap()		{return &mSamplerHeap;}
			dx12::Heap*					RenderTargetHeap()	{ return &mRenderTargetHeap; }
			dx12::Heap*					DepthStencilHeap()	{ return &mDepthStencilHeap; }

			crossplatform::Texture*		GetDummy2D()		{ return mDummy2D;}
			crossplatform::Texture*		GetDummy3D()		{ return mDummy3D; }

			/// Returns the currently applied input layout
			D3D12_INPUT_LAYOUT_DESC*	GetCurrentInputLayout() { return mCurInputLayout; }
			/// Sets the current input layout
			void						SetCurrentInputLayout(D3D12_INPUT_LAYOUT_DESC* l) { mCurInputLayout = l; }
			/// Returns the current render target pixel format
			DXGI_FORMAT					GetCurrentPixelFormat() { return mCurPixelFormat; }
			/// Sets the current render target pixel format
			void						SetCurrentPixelFormat(DXGI_FORMAT format) { mCurPixelFormat = format; }
			/// Returns the current applied primitive topology
			D3D_PRIMITIVE_TOPOLOGY		GetCurrentPrimitiveTopology() { return mStoredTopology; }

			void						RestoreDeviceObjects(void*);
			void						InvalidateDeviceObjects();
			void						RecompileShaders();

			D3D11_MAP_FLAG				GetMapFlags();
			bool						UsesFastSemantics() const
			{
				return !can_save_and_restore;
			}


			virtual void				BeginEvent(crossplatform::DeviceContext &deviceContext,const char *name);
			virtual void				EndEvent(crossplatform::DeviceContext &deviceContext);
			void						StartRender(crossplatform::DeviceContext &deviceContext);
			void						EndRender(crossplatform::DeviceContext &deviceContext);
			void						IntializeLightingEnvironment(const float pAmbientLight[3]);
			void						CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s);

			void						DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			
			void						ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int index);
			
			void						Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert);
			void						DrawIndexed	(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0) override;
			void						DrawMarker		(crossplatform::DeviceContext &deviceContext,const double *matrix);
			void						DrawCrossHair	(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition);
			void						DrawCamera		(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll);
			void						DrawLineLoop	(crossplatform::DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void						DrawTexture	(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend=false);
			void						DrawQuad		(crossplatform::DeviceContext &deviceContext);

			void						DrawLines		(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false);
			void						Draw2dLines	(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip);
			void						DrawCube		(crossplatform::DeviceContext &deviceContext);

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
			
			void									StoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									PushRenderTargets(crossplatform::DeviceContext &deviceContext);
			void									PopRenderTargets(crossplatform::DeviceContext &deviceContext);
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
			void									Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			bool									ApplyContextState(crossplatform::DeviceContext &deviceContext, bool error_checking = true) override;

			// DX11-specific stuff:
			static									DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p);
			static									crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
			crossplatform::ShaderResourceType		FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t);

		protected:
			ID3DUserDefinedAnnotation *pUserDefinedAnnotation;
			/// \todo The stored states are implemented per-RenderPlatform for DX11, but need to be implemented per-DeviceContext.
			struct StoredState
			{
				void Clear();
				UINT m_StencilRefStored11;
				UINT m_SampleMaskStored11;
				UINT m_indexOffset;
				DXGI_FORMAT m_indexFormatStored11;
				D3D_PRIMITIVE_TOPOLOGY m_previousTopology;
				ID3D11DepthStencilState* m_pDepthStencilStateStored11;
				ID3D11RasterizerState* m_pRasterizerStateStored11;
				ID3D11BlendState* m_pBlendStateStored11;
				ID3D11Buffer *pIndexBufferStored11;
				ID3D11InputLayout* m_previousInputLayout;
				float m_BlendFactorStored11[4];
				ID3D11SamplerState* m_pSamplerStateStored11[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
				ID3D11SamplerState* m_pVertexSamplerStateStored11[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
				ID3D11SamplerState* m_pComputeSamplerStateStored11[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
				ID3D11SamplerState* m_pGeometrySamplerStateStored11[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pCSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11UnorderedAccessView* m_pUnorderedAccessViews[D3D11_PS_CS_UAV_REGISTER_COUNT];
				ID3D11Buffer *m_pVertexBuffersStored11[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				UINT m_VertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				UINT m_VertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11Buffer *m_pCSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11Buffer *m_pGSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11Buffer *m_pPSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11Buffer *m_pVSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11Buffer *m_pHSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11Buffer *m_pDSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
				ID3D11VertexShader *pVertexShader;
				ID3D11PixelShader *pPixelShader;
				ID3D11HullShader *pHullShader;
				ID3D11DomainShader *pDomainShader;
				ID3D11GeometryShader *pGeometryShader;
				ID3D11ComputeShader *pComputeShader;
				ID3D11ClassInstance *m_pPixelClassInstances[16];
				UINT numPixelClassInstances;
				ID3D11ClassInstance *m_pVertexClassInstances[16];
				UINT numVertexClassInstances;
				ID3D11ClassInstance *m_pGeometryClassInstances[16];
				UINT numGeometryClassInstances;
				ID3D11ClassInstance *m_pHullClassInstances[16];
				UINT numHullClassInstances;
				ID3D11ClassInstance *m_pDomainClassInstances[16];
				UINT numDomainClassInstances;
				ID3D11ClassInstance *m_pComputeClassInstances[16];
				UINT numComputeClassInstances;
			};
			int storedStateCursor;
			std::vector<StoredState> storedStates;
			std::vector<struct RTState*> storedRTStates;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif