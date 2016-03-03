#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"

#pragma warning(disable:4251)

struct ID3D11Buffer;
struct ID3DX11EffectConstantBuffer;
struct ID3DX11EffectPass;
namespace simul
{
	namespace dx11
	{
		struct SIMUL_DIRECTX11_EXPORT Query:public crossplatform::Query
		{
			ID3D11Query *d3d11Query[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				for(int i=0;i<QueryLatency;i++)
					d3d11Query[i]		=0;
			}
			virtual ~Query() override
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;
			void Begin(crossplatform::DeviceContext &deviceContext) override;
			void End(crossplatform::DeviceContext &deviceContext) override;
			bool GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void SetName(const char *n) override;
		};
		struct SIMUL_DIRECTX11_EXPORT RenderState:public crossplatform::RenderState
		{
			ID3D11DepthStencilState		*m_depthStencilState;
			ID3D11BlendState			*m_blendState;
			RenderState();
			virtual ~RenderState();
		};
		// Platform-specific data for constant buffer, managed by RenderPlatform.
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			ID3D11Buffer*					m_pD3D11Buffer;
			ID3DX11EffectConstantBuffer*	m_pD3DX11EffectConstantBuffer;
		public:
			PlatformConstantBuffer():m_pD3D11Buffer(NULL),m_pD3DX11EffectConstantBuffer(NULL)
			{
			}
			inline ID3D11Buffer *asD3D11Buffer()
			{
				return m_pD3D11Buffer;
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r,size_t sz,void *addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			void Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void Unbind(simul::crossplatform::DeviceContext &deviceContext);
		};
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
			ID3D11Buffer						*buffer;
			ID3D11Buffer						**stagingBuffers;
			ID3D11ShaderResourceView			*shaderResourceView;
			ID3D11UnorderedAccessView			*unorderedAccessView;
			D3D11_MAPPED_SUBRESOURCE			mapped;
			int num_elements;
			int element_bytesize;
			ID3D11DeviceContext					*lastContext;
			unsigned char *read_data;
		public:
			PlatformStructuredBuffer();
			virtual ~PlatformStructuredBuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data);
			void *GetBuffer(crossplatform::DeviceContext &deviceContext);
			const void *OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			void CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			void CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void SetData(crossplatform::DeviceContext &deviceContext,void *data);
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return shaderResourceView;
			}
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int mip=0)
			{
				return unorderedAccessView;
			}
			void InvalidateDeviceObjects();
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name);
			void ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name);
			void Unbind(crossplatform::DeviceContext &deviceContext);
		};
		class SIMUL_DIRECTX11_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
			int NumPasses() const;
		};
		class SIMUL_DIRECTX11_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
			ID3DX11EffectPass *currentPass;
		public:
			Effect();
			virtual ~Effect();
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
			void SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex,int mip=0);
			
			void SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *tex,int mip=0);
			
			crossplatform::ShaderResource GetShaderResource(const char *name) override;
			void SetTexture		(crossplatform::DeviceContext &,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *t,int mip=-1) override;
			void SetTexture		(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex,int mip=-1);
			void SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s);
			void SetConstantBuffer(crossplatform::DeviceContext &deviceContext,const char *name	,crossplatform::ConstantBufferBase *s);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass);
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
		};
	}
}
