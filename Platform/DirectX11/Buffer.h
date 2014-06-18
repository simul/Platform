#pragma once
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include <string>
#include <map>
#include "SimulDirectXHeader.h"

#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Buffer:public simul::crossplatform::Buffer
		{
			ID3D11Buffer *d3d11Buffer;
		public:
			Buffer();
			~Buffer();
			ID3D11Buffer *AsD3D11Buffer();
			GLuint AsGLuint();
			void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data);
		};
	}
};

