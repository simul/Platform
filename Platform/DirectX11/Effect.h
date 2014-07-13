#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"

#pragma warning(disable:4251)

struct ID3D11Buffer;
struct ID3DX11EffectConstantBuffer;

namespace simul
{
	namespace dx11
	{
		// Platform-specific data for constant buffer, managed by RenderPlatform.
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			ID3D11Buffer*					m_pD3D11Buffer;
			ID3DX11EffectConstantBuffer*	m_pD3DX11EffectConstantBuffer;
		public:
			PlatformConstantBuffer():m_pD3D11Buffer(NULL),m_pD3DX11EffectConstantBuffer(NULL)
			{
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
			ID3D11ShaderResourceView			*shaderResourceView;
			ID3D11UnorderedAccessView			*unorderedAccessView;
			D3D11_MAPPED_SUBRESOURCE			mapped;
			int size;
			ID3D11DeviceContext					*lastContext;
		public:
			PlatformStructuredBuffer()
				:size(0)
				,buffer(0)
				,shaderResourceView(0)
				,unorderedAccessView(0)
			{
				memset(&mapped,0,sizeof(mapped));
			}
			virtual ~PlatformStructuredBuffer()
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int count,int unit_size,bool computable)
			{
				InvalidateDeviceObjects();
				D3D11_BUFFER_DESC sbDesc;
				memset(&sbDesc,0,sizeof(sbDesc));
				if(computable)
				{
					sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
					sbDesc.Usage				=D3D11_USAGE_DEFAULT;
					sbDesc.CPUAccessFlags		=0;
				}
				else
				{
				sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE ;
				sbDesc.Usage				=D3D11_USAGE_DYNAMIC;
				sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
				}
				sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
				sbDesc.StructureByteStride	=unit_size;
				sbDesc.ByteWidth			=unit_size *count;
				(renderPlatform->AsD3D11Device()->CreateBuffer( &sbDesc, NULL, &buffer ));

				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
				memset(&srv_desc,0,sizeof(srv_desc));
				srv_desc.Format						=DXGI_FORMAT_UNKNOWN;
				srv_desc.ViewDimension				=D3D11_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.ElementOffset		=0;
				srv_desc.Buffer.ElementWidth		=0;
				srv_desc.Buffer.FirstElement		=0;
				srv_desc.Buffer.NumElements			=size;
				renderPlatform->AsD3D11Device()->CreateShaderResourceView(buffer, &srv_desc,&shaderResourceView);

				if(computable)
				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
					memset(&uav_desc,0,sizeof(uav_desc));
					uav_desc.Format						=DXGI_FORMAT_UNKNOWN;
					uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_BUFFER;
					uav_desc.Buffer.FirstElement		=0;
					uav_desc.Buffer.Flags				=0;
					uav_desc.Buffer.NumElements			=size;
					(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(buffer, &uav_desc,&unorderedAccessView));
				}
			}
			void *GetBuffer(ID3D11DeviceContext *pContext)
			{
				lastContext=pContext;
				if(!mapped.pData)
					pContext->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
				void *ptr=(void *)mapped.pData;
				return ptr;
			}
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name);
			void Unbind(crossplatform::DeviceContext &deviceContext);
		};
		class SIMUL_DIRECTX11_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
			int NumPasses() const;
		};
		class SIMUL_DIRECTX11_EXPORT Effect:public simul::crossplatform::Effect
		{
		public:
			Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			Effect();
			virtual ~Effect();
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
			void SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex);
			void SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex);
			void SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture &t);
			void SetTexture(const char *name,ID3D11ShaderResourceView *tex);
			void SetParameter	(const char *name	,float value)		;
			void SetParameter	(const char *name	,vec2 value)		;
			void SetParameter	(const char *name	,vec3 value)		;
			void SetParameter	(const char *name	,vec4 value)		;
			void SetParameter	(const char *name	,int value)			;
			void SetVector		(const char *name	,const float *vec)	;
			void SetMatrix		(const char *name	,const float *m)	;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass);
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
		};
	}
}
