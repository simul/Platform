#pragma once
#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include <string>
#include <map>
#include "DirectXHeader.h"

#pragma warning(disable:4251)

struct ID3D11Buffer;
struct ID3DX11EffectConstantBuffer;
struct ID3DX11EffectPass;
namespace platform
{
	namespace dx11
	{
		//! Platform-specific data for constant buffer, managed by RenderPlatform.
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			ID3D11Buffer *m_pD3D11Buffer;
	#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
			// each context will have its own index into the array of buffers.
			std::vector<unsigned > m_index;
			// TODO: All this should be PER-CONTEXT:
			__int64 framenumber;
			UINT num_this_frame;
			BYTE* m_pPlacementBuffer;
	#endif
			UINT byteWidth;
			UINT m_nContexts;
			UINT m_nBuffering;
			UINT m_nFramesBuffering;
			void *last_placement;
			bool resize;
		public:
			//! Constructor
			PlatformConstantBuffer();
			//! Destructor
			~PlatformConstantBuffer();
			ID3D11Buffer *asD3D11Buffer();
			//! Platform-dependent function called when initializing the constant buffer.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r,size_t sz,void *addr);
			//! Platform-dependent function called when uninitializing the constant buffer.
			void InvalidateDeviceObjects();
			//! Find the constant buffer in the given effect, and link to it.
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			//! Apply the current values within the constant buffer
			void Apply(platform::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			//! Unbind the constant buffer
			void Unbind(platform::crossplatform::DeviceContext &deviceContext);
			
			void CreateBuffers( crossplatform::RenderPlatform* r, void *addr);

			void *GetBaseAddr();
		public:
			void SetNumBuffers(crossplatform::RenderPlatform* r,UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering );
		};
	}
}
