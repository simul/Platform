#pragma once
 
#include "Simul/Platform/CrossPlatform/Export.h"
struct ID3D11Buffer;
typedef unsigned GLuint;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Buffer
		{
		public:
			Buffer();
			virtual ~Buffer();
			virtual ID3D11Buffer *AsD3D11Buffer()=0;
			virtual GLuint AsGLuint()=0;
			virtual void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data)=0;
			int stride;
		};
	}
}
