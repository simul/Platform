#pragma once

#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "DirectXHeader.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class PlatformStructuredBuffer :public crossplatform::PlatformStructuredBuffer
		{
			ID3D11Buffer* buffer;
			ID3D11Buffer** stagingBuffers;
			ID3D11ShaderResourceView* shaderResourceView;
			ID3D11UnorderedAccessView* unorderedAccessView;
			D3D11_MAPPED_SUBRESOURCE			mapped;
			int num_elements;
			int element_bytesize;
			ID3D11DeviceContext* lastContext;
			unsigned char* read_data;
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
			void RestoreDeviceObjects(crossplatform::RenderPlatform* renderPlatform, int ct, int unit_size, bool computable, bool cpu_read, void* init_data, const char* name, crossplatform::BufferUsageHint b);
			void InvalidateDeviceObjects();
			void* GetBuffer(crossplatform::DeviceContext& deviceContext);
			const void* OpenReadBuffer(crossplatform::DeviceContext& deviceContext);
			void CloseReadBuffer(crossplatform::DeviceContext& deviceContext);
			void CopyToReadBuffer(crossplatform::DeviceContext& deviceContext);
			void SetData(crossplatform::DeviceContext& deviceContext, void* data);
			ID3D11ShaderResourceView* AsD3D11ShaderResourceView()
			{
				return shaderResourceView;
			}
			ID3D11UnorderedAccessView* AsD3D11UnorderedAccessView()
			{
				return unorderedAccessView;
			}
			void Apply(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& shaderResource);
			void ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,  const crossplatform::ShaderResource& shaderResource);

			void Unbind(crossplatform::DeviceContext& deviceContext);
		};
	}
}