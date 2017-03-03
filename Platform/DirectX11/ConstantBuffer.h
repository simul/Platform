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
		// Platform-specific data for constant buffer, managed by RenderPlatform.
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			ID3D11Buffer *m_pD3D11Buffer;
			ID3DX11EffectConstantBuffer*	m_pD3DX11EffectConstantBuffer;
	#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
			// each context will have its own index into the array of buffers.
			std::vector<unsigned > m_index;
			// TODO: All this should be PER-CONTEXT:
			__int64 framenumber;
			int num_this_frame;
			BYTE* m_pPlacementBuffer;
	#endif
			UINT byteWidth;
			UINT m_nContexts;
			UINT m_nBuffering;
			UINT m_nFramesBuffering;
			void *last_placement;
			bool resize;
		public:
			PlatformConstantBuffer();
			~PlatformConstantBuffer();
			ID3D11Buffer *asD3D11Buffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r,size_t sz,void *addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			void Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void Unbind(simul::crossplatform::DeviceContext &deviceContext);
			
			void CreateBuffers( crossplatform::RenderPlatform* r, void *addr);

			void *GetBaseAddr();
		public:
			void SetNumBuffers(crossplatform::RenderPlatform* r,UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering );
		};
	}
}
