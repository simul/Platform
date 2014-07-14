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
			int num_elements;
			int element_bytesize;
			ID3D11DeviceContext					*lastContext;
		public:
			PlatformStructuredBuffer()
				:num_elements(0)
				,element_bytesize(0)
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
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data);
			void *GetBuffer(crossplatform::DeviceContext &deviceContext);
			void SetData(crossplatform::DeviceContext &deviceContext,void *data);
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return shaderResourceView;
			}
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()
			{
				return unorderedAccessView;
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
