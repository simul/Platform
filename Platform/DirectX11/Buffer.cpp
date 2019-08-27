#include "Buffer.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SimulDirectXHeader.h"
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

 ID3D11Buffer *const Buffer::AsD3D11Buffer() const
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

void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const crossplatform::Layout *layout,const void *data,bool cpu_access,bool streamout_target)
{
    D3D11_SUBRESOURCE_DATA InitData;
    memset( &InitData,0,sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem		=data;
    InitData.SysMemPitch	=layout->GetStructSize();
	D3D11_USAGE usage			=D3D11_USAGE_DYNAMIC;
	if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
		usage=D3D11_USAGE_DEFAULT;
	D3D11_BUFFER_DESC desc=
	{
		(unsigned)(num_vertices*layout->GetStructSize())
		,cpu_access?usage:D3D11_USAGE_DEFAULT
		,D3D11_BIND_VERTEX_BUFFER|(streamout_target?D3D11_BIND_STREAM_OUTPUT: (unsigned)0)
		,(cpu_access?D3D11_CPU_ACCESS_WRITE: (unsigned)0),(unsigned)0
	};
	SAFE_RELEASE(d3d11Buffer);
	V_CHECK(renderPlatform->AsD3D11Device()->CreateBuffer(&desc,data?&InitData:NULL,&d3d11Buffer));
	stride=layout->GetStructSize();
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

void *Buffer::Map(crossplatform::DeviceContext &deviceContext)
{
	D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
	if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
		map_type=D3D11_MAP_WRITE;
	V_CHECK(deviceContext.asD3D11DeviceContext()->Map(d3d11Buffer,0,map_type,0,&mapped));
	return (void*)mapped.pData;
}

void Buffer::Unmap(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.asD3D11DeviceContext()->Unmap(d3d11Buffer,0);
}