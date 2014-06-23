#include <GL/glew.h>
#include "Buffer.h"

using namespace simul;
using namespace opengl;

Buffer::Buffer()
	:buf(0)
{
}


Buffer::~Buffer()
{
}


ID3D11Buffer *Buffer::AsD3D11Buffer()
{
	return NULL;
}

GLuint Buffer::AsGLuint()
{
	return buf;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data)
{
    glGenBuffers(1, &buf);
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, num_vertices * struct_size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
	stride=struct_size;
}