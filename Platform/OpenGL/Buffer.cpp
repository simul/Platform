#include <GL/glew.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "Buffer.h"
#include "SimulGLUtilities.h"
#include "RenderPlatform.h"
#include "Simul/Base/RuntimeError.h"

using namespace simul;
using namespace opengl;

Buffer::Buffer()
	:buf(0),vao(0),mapped(NULL)
{
}


Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(buf)
	SAFE_DELETE_VAO(vao)
}


ID3D11Buffer *Buffer::AsD3D11Buffer()
{
	return NULL;
}

GLuint Buffer::AsGLuint()
{
	return buf;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access,bool streamout_target)
{
	SAFE_DELETE_BUFFER(buf)
    glGenBuffers(1, &buf);
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, buf);
	//usage Specifies the expected usage pattern of the data store. The symbolic constant must be
	//GL_STREAM_DRAW, GL_STREAM_READ, GL_STREAM_COPY, GL_STATIC_DRAW, GL_STATIC_READ, GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, or GL_DYNAMIC_COPY.
    glBufferData(GL_ARRAY_BUFFER, num_vertices * layout->GetStructSize(), data,(cpu_access?GL_DYNAMIC_DRAW:GL_STATIC_DRAW));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
	stride=layout->GetStructSize();

GL_ERROR_CHECK
	SAFE_DELETE_VAO(vao);
	glGenVertexArrays(1,&vao );
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER,buf);
	for(int i=0;i<layout->GetDesc().size();i++)
	{
		const crossplatform::LayoutDesc &d=layout->GetDesc()[i];
		glEnableVertexAttribArray( i );
		glVertexAttribPointer( i						// Attribute bind location
								,RenderPlatform::FormatCount(d.format)	// Data type count
								,RenderPlatform::DataType(d.format)				// Data type
								,GL_FALSE				// Normalise this data type?
								,stride			// Stride to the next vertex
								,(GLvoid*)d.alignedByteOffset );	// Vertex Buffer starting offset
	};
	
	glBindVertexArray( 0 ); 
GL_ERROR_CHECK
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *,int num_indices,int index_size_bytes,const void *data)
{
	SAFE_DELETE_BUFFER(buf)
    glGenBuffers(1, &buf);
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER,buf);
    glBufferData(GL_ARRAY_BUFFER,num_indices * index_size_bytes, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,0);
	stride=index_size_bytes;
}

void *Buffer::Map(crossplatform::DeviceContext &)
{
	if(mapped!=NULL)
		SIMUL_BREAK("Buffer::Map - Already mapped");
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	mapped = (void*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	return mapped;
}

void Buffer::Unmap(crossplatform::DeviceContext &)
{
	// if the pointer is valid(mapped), update VBO
	if(mapped)
	{
		glUnmapBuffer(GL_ARRAY_BUFFER); // unmap it after use
		mapped=NULL;
	}
}
