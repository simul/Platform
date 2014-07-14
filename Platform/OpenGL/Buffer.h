#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include <string>
#include <map>

#pragma warning(disable:4251)

namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Buffer:public simul::crossplatform::Buffer
		{
			GLuint buf;
		public:
			Buffer();
			~Buffer();
			void InvalidateDeviceObjects();
			ID3D11Buffer *AsD3D11Buffer();
			GLuint AsGLuint();
			void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data);
			void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data);
		};
	}
};

