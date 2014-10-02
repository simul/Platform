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
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11Buffer *AsD3D11Buffer()=0;
			virtual GLuint AsGLuint()=0;
			//! Set up as a vertex buffer.
			virtual void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data,bool cpu_access=false)=0;
			//! Set up as an index buffer.
			virtual void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data)=0;
			//! Get a pointer to the data for updating. Must call Unmap after any changes.
			virtual void *Map(crossplatform::DeviceContext &deviceContext) =0;
			//! Return the modified data to the device object.
			virtual void Unmap(crossplatform::DeviceContext &deviceContext) =0;
			int stride;
		};
	}
}
