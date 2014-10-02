#include <GL/glew.h>
#include "Buffer.h"
#include "SimulGLUtilities.h"

using namespace simul;
using namespace opengl;

Buffer::Buffer()
	:buf(0)
{
}


Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(buf)
}


ID3D11Buffer *Buffer::AsD3D11Buffer()
{
	return NULL;
}

GLuint Buffer::AsGLuint()
{
	return buf;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data,bool cpu_access,bool streamout_target)
{
	SAFE_DELETE_BUFFER(buf)
    glGenBuffers(1, &buf);
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, buf);
	//usage Specifies the expected usage pattern of the data store. The symbolic constant must be
	//GL_STREAM_DRAW, GL_STREAM_READ, GL_STREAM_COPY, GL_STATIC_DRAW, GL_STATIC_READ, GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, or GL_DYNAMIC_COPY.
    glBufferData(GL_ARRAY_BUFFER, num_vertices * struct_size, data,(cpu_access?GL_DYNAMIC_DRAW:GL_STATIC_DRAW));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
	stride=struct_size;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data)
{
	SAFE_DELETE_BUFFER(buf)
    glGenBuffers(1, &buf);
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER,buf);
    glBufferData(GL_ARRAY_BUFFER,num_indices * index_size_bytes, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
	stride=index_size_bytes;
}

void *Buffer::Map(crossplatform::DeviceContext &deviceContext)
{
	return NULL;
}

void Buffer::Unmap(crossplatform::DeviceContext &deviceContext)
{
}
