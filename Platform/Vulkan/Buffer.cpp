#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "Buffer.h"
#include "RenderPlatform.h"
#include "Simul/Base/RuntimeError.h"

using namespace simul;
using namespace vulkan;

Buffer::Buffer()
{
}

Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
}


void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_vertices,const crossplatform::Layout* layout,const void* data,bool cpu_access,bool streamout_target)
{
    InvalidateDeviceObjects();
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_indices,int index_size_bytes,const void* data)
{
    InvalidateDeviceObjects();
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
    return 0;
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
}
