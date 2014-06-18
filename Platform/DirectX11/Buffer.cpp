#include "Buffer.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
using namespace simul;
using namespace dx11;

Buffer::Buffer()
	:d3d11Buffer(NULL)
{
}


Buffer::~Buffer()
{
	SAFE_RELEASE(d3d11Buffer);
}

ID3D11Buffer *Buffer::AsD3D11Buffer()
{
	return d3d11Buffer;
}

GLuint Buffer::AsGLuint()
{
	return 0;
}

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,int struct_size,const void *data)
{
    D3D11_SUBRESOURCE_DATA InitData;
    memset( &InitData,0,sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem		=data;
    InitData.SysMemPitch	=struct_size;
	D3D11_BUFFER_DESC desc=
	{
        num_vertices*struct_size,D3D11_USAGE_DEFAULT,D3D11_BIND_VERTEX_BUFFER,0,0
	};
	renderPlatform->AsD3D11Device()->CreateBuffer(&desc,&InitData,&d3d11Buffer);
}