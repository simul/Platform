#pragma once
#include "Simul/Platform/DirectX11on12/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"
#include "Simul/Platform/DirectX11on12/ConstantBuffer.h"

#pragma warning(disable:4251)

struct ID3D11Buffer;
struct ID3DX11EffectConstantBuffer;
struct ID3DX11EffectPass;
namespace simul
{
	namespace dx11on12
	{
		struct SIMUL_DIRECTX12_EXPORT Query:public crossplatform::Query
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
		struct SIMUL_DIRECTX12_EXPORT RenderState:public crossplatform::RenderState
		{
			ID3D11DepthStencilState		*m_depthStencilState;
			ID3D11BlendState			*m_blendState;
			RenderState();
			virtual ~RenderState();
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
	#ifdef _XBOX_ONE
			BYTE* m_pPlacementBuffer;
			UINT byteWidth;
			std::vector< UINT > m_index;
	#endif
			UINT m_nContexts;
			UINT m_nObjects;
			UINT m_nBuffering;
			UINT iObject;
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
			void Unbind(crossplatform::DeviceContext &deviceContext);
		};
		class SIMUL_DIRECTX12_EXPORT EffectPass:public simul::crossplatform::EffectPass
		{
		public:
			void Apply(crossplatform::DeviceContext &deviceContext,bool test) override;
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
				ID3D11GeometryShader		*geometryShader;
				ID3D11VertexShader			*vertexShader;
				ID3D11PixelShader			*pixelShader;
				ID3D11ComputeShader			*computeShader;
			};
		};
		class SIMUL_DIRECTX12_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
		public:
			Effect();
			virtual ~Effect();
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);

			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass) override;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass) override;
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext) override;
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
		};
	}
}
