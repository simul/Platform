#pragma once
#include "Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/BaseRenderer.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/solid_constants.sl"
#include "Platform/CrossPlatform/Shaders/debug_constants.sl"

#include "DirectXHeader.h"
#include <vector>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
#ifndef _XBOX_ONE
	struct ID3DUserDefinedAnnotation;
#endif

namespace platform
{
	namespace crossplatform
	{
		class Material;
		class ConstantBufferBase;
	}
	namespace dx11
	{
		class ConstantBufferCache;
		class Material;
		//! A class to implement common rendering functionality for DirectX 11.
		class SIMUL_DIRECTX11_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
			ID3D11Device*					device;
			UINT64 fence;
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			const char *GetName() const
			{
				return "DirectX 11";
			}
			crossplatform::RenderPlatformType GetType() const override;
			virtual const char* GetSfxConfigFilename() const override
			{
				return "Sfx/DirectX11.json";
			}
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();

			D3D11_MAP_FLAG GetMapFlags();

			bool UsesFastSemantics() const
			{
				return !can_save_and_restore;
			}
			ID3D11Device *AsD3D11Device() override;
			virtual void BeginEvent			(crossplatform::DeviceContext &deviceContext,const char *name);
			virtual void EndEvent			(crossplatform::DeviceContext &deviceContext);
			void CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s);

			void DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			
			void ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int index);
			
			void Draw			(crossplatform::GraphicsDeviceContext &deviceContext,int num_verts,int start_vert);
			void DrawIndexed	(crossplatform::GraphicsDeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0) override;
			void DrawQuad		(crossplatform::GraphicsDeviceContext &deviceContext);

			
			void ApplyDefaultMaterial();
			crossplatform::Framebuffer			*CreateFramebuffer(const char *name=nullptr) override;
			crossplatform::SamplerState				*CreateSamplerState(crossplatform::SamplerStateDesc *d);
			crossplatform::Effect					*CreateEffect();
			crossplatform::PlatformConstantBuffer	*CreatePlatformConstantBuffer();
			crossplatform::PlatformStructuredBuffer	*CreatePlatformStructuredBuffer();
			crossplatform::Buffer					*CreateBuffer();
			crossplatform::Layout					*CreateLayout(int num_elements,const crossplatform::LayoutDesc *,bool interleaved);			
			crossplatform::RenderState				*CreateRenderState(const crossplatform::RenderStateDesc &desc);
			crossplatform::Query					*CreateQuery(crossplatform::QueryType q) override;
			crossplatform::Shader					*CreateShader() override;
            crossplatform::DisplaySurface*          CreateDisplaySurface();

			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,const crossplatform::Buffer *const*buffers,const crossplatform::Layout *layout,const int *vertexSteps=NULL) override;
			void									ClearVertexBuffers(crossplatform::DeviceContext& deviceContext) override;
			void									SetStreamOutTarget(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Buffer *buffer,int start_index=0);

			void									ApplyDefaultRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext);
			void									ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth);
			void									DeactivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext) override;
		
			void									ExecuteCommands(crossplatform::DeviceContext &deviceContext) override;
			void									RestartCommands(crossplatform::DeviceContext &deviceContext) override;
		//	crossplatform::Viewport					GetViewport(crossplatform::DeviceContext &deviceContext,int index);
			void									SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps);
			void									SetScissor(crossplatform::GraphicsDeviceContext &deviceContext,int4 sc) override;
			void									SetIndexBuffer(crossplatform::GraphicsDeviceContext &deviceContext, const crossplatform::Buffer *buffer) override;
			
			void									SetTopology(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Topology t) override;
			void									SetLayout(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Layout *l) override;
			
			void									StoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									RestoreRenderState(crossplatform::DeviceContext &deviceContext);
			void									PopRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext);
			void									SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s) override;
			void									Resolve(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source) override;
			void									SaveTexture(crossplatform::GraphicsDeviceContext&, crossplatform::Texture *texture,const char *lFileNameUtf8) override;
			// DX11-specific stuff:
			static DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p, crossplatform::CompressionFormat c=crossplatform::CompressionFormat::UNCOMPRESSED);
			
			static crossplatform::CompressionFormat DxgiFormatToCompressionFormat(DXGI_FORMAT dxgi_format);
			static D3D11_COMPARISON_FUNC toD3dComparison(crossplatform::DepthComparison d);
			static crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
			crossplatform::ShaderResourceType FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t);
		protected:
			void ContextFrameBegin(crossplatform::GraphicsDeviceContext& deviceContext) override;
			crossplatform::Texture* createTexture();
			bool ApplyContextState(crossplatform::DeviceContext &deviceContext, bool /*error_checking*/ = true) override;
			void WaitForFencedResources(crossplatform::DeviceContext &deviceContext);
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
				D3D11_RECT m_scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
				UINT m_numRects;
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
				ID3D11ShaderResourceView* m_pPSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pCSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pVSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pHSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pGSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11ShaderResourceView* m_pDSShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
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
				ID3D11ClassInstance *m_pPixelClassInstances[32];
				UINT numPixelClassInstances;
				ID3D11ClassInstance *m_pVertexClassInstances[32];
				UINT numVertexClassInstances;
				ID3D11ClassInstance *m_pGeometryClassInstances[32];
				UINT numGeometryClassInstances;
				ID3D11ClassInstance *m_pHullClassInstances[32];
				UINT numHullClassInstances;
				ID3D11ClassInstance *m_pDomainClassInstances[32];
				UINT numDomainClassInstances;
				ID3D11ClassInstance *m_pComputeClassInstances[32];
				UINT numComputeClassInstances;
			};
			int storedStateCursor;
			std::vector<StoredState> storedStates;

};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif