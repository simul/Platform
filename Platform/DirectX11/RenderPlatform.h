#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"

#include "SimulDirectXHeader.h"
#include <vector>

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
	namespace dx11
	{
		class ConstantBufferCache;
		class Material;
		//! A class to implement common rendering functionality for DirectX 11.
		class SIMUL_DIRECTX11_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
			ID3D11Device*					device;
			ID3D11InputLayout				*m_pCubemapVtxDecl;
			ID3D11Buffer					*m_pVertexBuffer;
			ID3D11InputLayout*				m_pVtxDecl;
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			const char *GetName() const
			{
				return "DirectX 11";
			}
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			ID3D11Device *AsD3D11Device()
			{
				return device;
			}
			virtual void BeginEvent			(crossplatform::DeviceContext &deviceContext,const char *name);
			virtual void EndEvent			(crossplatform::DeviceContext &deviceContext);
			void StartRender(crossplatform::DeviceContext &deviceContext);
			void EndRender(crossplatform::DeviceContext &deviceContext);
			void IntializeLightingEnvironment(const float pAmbientLight[3]);
			void CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s);

			void DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			
			void ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int index);
			
			void Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert);
			void DrawIndexed	(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0) override;
			void DrawMarker		(crossplatform::DeviceContext &deviceContext,const double *matrix);
			void DrawCrossHair	(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition);
			void DrawCamera		(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop	(crossplatform::DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void DrawTexture	(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend=false);
			void DrawDepth		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v=NULL);
			void DrawQuad		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique,const char *pass=NULL);
			void DrawQuad		(crossplatform::DeviceContext &deviceContext);

			void DrawLines		(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false);
			void Draw2dLines	(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip);
			void DrawCircle		(crossplatform::DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false);
			void DrawCube		(crossplatform::DeviceContext &deviceContext);
			void DrawCubemap	(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *cubemap,float offsetx,float offsety,float exposure,float gamma);

			void ApplyDefaultMaterial();
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

			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer **buffers,const crossplatform::Layout *layout,const int *vertexSteps=NULL);
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
			// DX11-specific stuff:
			static DXGI_FORMAT ToDxgiFormat(crossplatform::PixelFormat p);
			static crossplatform::PixelFormat FromDxgiFormat(DXGI_FORMAT f);
			crossplatform::ShaderResourceType FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t);
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
				ID3D11ShaderResourceView* m_pShaderResourceViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
				ID3D11UnorderedAccessView* m_pUnorderedAccessViews[D3D11_PS_CS_UAV_REGISTER_COUNT];
				ID3D11Buffer *m_pVertexBuffersStored11[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				UINT m_VertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				UINT m_VertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
				
				ID3D11VertexShader *pVertexShader;
				ID3D11PixelShader *pPixelShader;
				ID3D11ClassInstance *m_pPixelClassInstances[16];
				UINT numPixelClassInstances;
				ID3D11ClassInstance *m_pVertexClassInstances[16];
				UINT numVertexClassInstances;
			};
			int storedStateCursor;
			std::vector<StoredState> storedStates;
			std::vector<struct RTState*> storedRTStates;
			void DrawTexture	(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *tex,vec4 mult,bool blend=false);
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif