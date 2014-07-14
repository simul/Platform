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
	InvalidateDeviceObjects();
}

ID3D11Buffer *Buffer::AsD3D11Buffer()
{
	return d3d11Buffer;
}

GLuint Buffer::AsGLuint()
{
	return 0;
}

void Buffer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(d3d11Buffer);
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
	SAFE_RELEASE(d3d11Buffer);
	renderPlatform->AsD3D11Device()->CreateBuffer(&desc,&InitData,&d3d11Buffer);
	stride=struct_size;
}

void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void *data)
{
	D3D11_BUFFER_DESC ib_desc;
	ib_desc.ByteWidth			= num_indices * index_size_bytes;
	ib_desc.Usage				= D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags			= D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags		= 0;
	ib_desc.MiscFlags			= 0;
	ib_desc.StructureByteStride = index_size_bytes;

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem			= data;
	init_data.SysMemPitch		= 0;
	init_data.SysMemSlicePitch	= 0;

	SAFE_RELEASE(d3d11Buffer);
	renderPlatform->AsD3D11Device()->CreateBuffer(&ib_desc, &init_data, &d3d11Buffer);
	stride=index_size_bytes;
}