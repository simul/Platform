#ifdef _MSC_VER
    #include <windows.h>
#endif
#include "Buffer.h"
#include "RenderPlatform.h"
#include "Platform/Core/RuntimeError.h"

using namespace platform;
using namespace opengl;

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

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_vertices,int structSize,std::shared_ptr<std::vector<uint8_t>> data,bool cpu_access,bool streamout_target)
{
    InvalidateDeviceObjects();

    glGenBuffers(1, &mBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, mBufferID);
    glBufferData(GL_ARRAY_BUFFER, structSize * num_vertices, data?data->data():nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform* renderPlatform,int num_indices,int index_size_bytes,std::shared_ptr<std::vector<uint8_t>> data, bool cpu_access)
{
    InvalidateDeviceObjects();

    glGenBuffers(1, &mBufferID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mBufferID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size_bytes * num_indices, data?data->data():nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void* Buffer::Map(crossplatform::DeviceContext& deviceContext)
{
    return glMapNamedBuffer(mBufferID, GL_READ_WRITE);
}

void Buffer::Unmap(crossplatform::DeviceContext& deviceContext)
{
    glUnmapNamedBuffer(mBufferID);
}

void Buffer::BindVBO(crossplatform::DeviceContext & deviceContext, const crossplatform::Layout* layout)
{
    mBufferLayout = (crossplatform::Layout*)layout;

    glBindBuffer(GL_ARRAY_BUFFER,mBufferID);
    auto bufferDesc     = mBufferLayout->GetDesc();
    size_t off          = 0;
    unsigned int cnt    = 0;
    for (auto ele : bufferDesc)
    {
        glVertexAttribPointer
        (
            cnt,
            opengl::RenderPlatform::FormatCount(ele.format),
            opengl::RenderPlatform::DataType(ele.format),
            GL_FALSE,
            mBufferLayout->GetStructSize(),
            (void*)off
        );
        glEnableVertexAttribArray(cnt);
        off += crossplatform::GetByteSize(ele.format);
        cnt++;
    }
}
