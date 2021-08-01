#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "Buffer.h"
#include "RenderPlatform.h"
#include "Platform/Core/RuntimeError.h"
#include <EGL/egl.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <GLES3/gl2ext.h>

using namespace simul;
using namespace gles;

Buffer::Buffer():
    mBufferID(0)
{
}

Buffer::~Buffer()
{
	InvalidateDeviceObjects();
}

void Buffer::InvalidateDeviceObjects()
{
    if(mBufferID != 0)
    {
        glDeleteBuffers(1, &mBufferID);
        mBufferID = 0;
    }
}

GLuint Buffer::AsGLuint()
{
	return mBufferID;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_vertices,const crossplatform::Layout* layout,const void* data,bool cpu_access,bool streamout_target)
{
    InvalidateDeviceObjects();

    glGenBuffers(1, &mBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, mBufferID);
    glBufferData(GL_ARRAY_BUFFER, layout->GetStructSize() * num_vertices, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    mBufferLayout = (crossplatform::Layout*)layout;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_indices,int index_size_bytes,const void* data)
{
    InvalidateDeviceObjects();

    glGenBuffers(1, &mBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size_bytes * num_indices, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
    //mBufferID
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBufferID);
    return glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBufferID);
    glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
}

void Buffer::BindVBO(crossplatform::DeviceContext & deviceContext)
{
    glBindBuffer(GL_ARRAY_BUFFER,mBufferID);
    auto bufferDesc     = mBufferLayout->GetDesc();
    size_t off          = 0;
    unsigned int cnt    = 0;
    for (auto ele : bufferDesc)
    {
        glVertexAttribPointer
        (
            cnt,
            gles::RenderPlatform::FormatCount(ele.format),
            gles::RenderPlatform::DataType(ele.format),
            GL_FALSE,
            mBufferLayout->GetStructSize(),
            (void*)off
        );
        glEnableVertexAttribArray(cnt);
        off += crossplatform::GetByteSize(ele.format);
        cnt++;
    }
}
