#pragma once
#include "Platform/DirectX11/Export.h"
#include "Platform/CrossPlatform/Buffer.h"
#include <string>
#include <map>
#include "DirectXHeader.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Buffer:public simul::crossplatform::Buffer
		{
			ID3D11Buffer *d3d11Buffer;
			D3D11_MAPPED_SUBRESOURCE mapped;
		public:
			Buffer();
			~Buffer();
			void InvalidateDeviceObjects();
			ID3D11Buffer *AsD3D11Buffer();
			ID3D11Buffer  *const AsD3D11Buffer() const;
			GLuint AsGLuint();
			void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access=false,bool streamout_target=false);
			void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data);
			void *Map(crossplatform::DeviceContext &deviceContext) override;
			void Unmap(crossplatform::DeviceContext &deviceContext) override;
		};
	}
};

