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
			void RestoreDeviceObjects(void *dev,size_t sz,void *addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name);
			void Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void Unbind(simul::crossplatform::DeviceContext &deviceContext);
		};
		class SIMUL_DIRECTX11_EXPORT Effect:public simul::crossplatform::Effect
		{
		public:
			Effect(void *device,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			Effect();
			~Effect();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			void SetTexture(const char *name,void *tex);
			void SetTexture(const char *name,crossplatform::Texture &t);
		};
	}
}
