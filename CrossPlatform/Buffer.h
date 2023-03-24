#pragma once
 
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Layout.h"
#include <string>
#include <vector>
#include <memory>

struct ID3D11Buffer;
struct ID3D12Resource;
struct D3D12_VERTEX_BUFFER_VIEW;
struct D3D12_INDEX_BUFFER_VIEW;
typedef unsigned GLuint;
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace crossplatform
	{
		enum class BufferType
		{
			UNKNOWN,
			INDEX,
			VERTEX
		};
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Buffer
		{
		public:
			Buffer();
			void SetName(const char* n);
			virtual ~Buffer();
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11Buffer *AsD3D11Buffer()
			{
				return nullptr;
			}
			virtual ID3D11Buffer * const AsD3D11Buffer() const
			{
				return nullptr;
			}
			virtual ID3D12Resource * const AsD3D12Buffer() const
			{
				return nullptr;
			}
			virtual ID3D12Resource *  AsD3D12Buffer() 
			{
				return nullptr;
			}
			virtual GLuint AsGLuint()
			{
				return 0;
			}
			BufferType GetBufferType()
			{
				return bufferType;
			}
			//! Set up as a vertex buffer. You must pass a pointer to an already-created Layout, and don't destroy the layout until after destroying the vertex buffer. This version of the function
			//! takes a copy of the data. To avoid this, use the version that takes a shared_ptr.
			virtual void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const Layout *layout,const void *data,bool cpu_access=false,bool streamout_target=false) final;
			//! Set up as an index buffer. This version of the function
			//! takes a copy of the data. To avoid this, use the version that takes a shared_ptr.
			virtual void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void * data, bool cpu_access = false) final;
			
			//! Set up as a vertex buffer. You must pass a pointer to an already-created Layout, and don't destroy the layout until after destroying the vertex buffer.
			virtual void EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const Layout *layout,std::shared_ptr<std::vector<uint8_t>>,bool cpu_access=false,bool streamout_target=false)=0;
			//! Set up as an index buffer.
			virtual void EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>>, bool cpu_access = false)=0;
			
			//! Get a pointer to the data for updating. Must call Unmap after any changes.
			virtual void *Map(crossplatform::DeviceContext &deviceContext) =0;
			//! Return the modified data to the device object.
			virtual void Unmap(crossplatform::DeviceContext &deviceContext) =0;
			int stride;
			int count;
			virtual D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView() { return NULL; }
			virtual D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView() { return NULL; }
		protected:
			BufferType bufferType= BufferType::UNKNOWN;
            crossplatform::Layout*  mBufferLayout=nullptr;
			crossplatform::RenderPlatform *renderPlatform=nullptr;
			std::string name;
			std::shared_ptr<std::vector<uint8_t>> upload_data;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif