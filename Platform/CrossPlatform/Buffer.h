#pragma once
 
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
struct ID3D11Buffer;
struct D3D12_VERTEX_BUFFER_VIEW;
struct D3D12_INDEX_BUFFER_VIEW;
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
			virtual ID3D11Buffer *AsD3D11Buffer()
			{
				return NULL;
			}
			virtual GLuint AsGLuint()
			{
				return 0;
			}
			virtual void GetVertices(DeviceContext &deviceContext,void *v) {}
			virtual void GetIndices(DeviceContext &deviceContext, void *i) {}
			//! Set up as a vertex buffer. You must pass a pointer to an already-created Layout, and don't destroy the layout until after destroying the vertex buffer.
			virtual void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const Layout *layout,const void *data,bool cpu_access=false,bool streamout_target=false)=0;
			//! Set up as an index buffer.
			virtual void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data)=0;
			//! Get a pointer to the data for updating. Must call Unmap after any changes.
			virtual void *Map(crossplatform::DeviceContext &deviceContext) =0;
			//! Return the modified data to the device object.
			virtual void Unmap(crossplatform::DeviceContext &deviceContext) =0;
			int stride;
			int count;
			virtual D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView() { return NULL; }
			virtual D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() { return NULL; }
		};
	}
}
