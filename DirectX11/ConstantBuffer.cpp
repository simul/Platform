#include "Platform/DirectX11/ConstantBuffer.h"
#include "Utilities.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

#include <string>

using namespace simul;
using namespace dx11;
#pragma optimize("",off)

inline bool IsPowerOfTwo( UINT64 n )
{
	return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
   SIMUL_ASSERT( IsPowerOfTwo(multiple) );

	return (value + multiple - 1) & ~(multiple - 1);
}

template< class T > UINT64 BytePtrToUint64( _In_ T* ptr )
{
	return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}



PlatformConstantBuffer::PlatformConstantBuffer() :
	m_pD3D11Buffer(NULL)
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	,framenumber(0)
	,num_this_frame(0)
	,m_pPlacementBuffer( nullptr )
#endif
	,byteWidth( 0 )
	,m_nContexts( 0 )
	,m_nBuffering( 0 )
	,m_nFramesBuffering(0)
	,last_placement(nullptr)
	,resize(false)
{
}
PlatformConstantBuffer::~PlatformConstantBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformConstantBuffer::CreateBuffers(crossplatform::RenderPlatform* r, void *addr) 
{
	renderPlatform=r;
	resize=false;
	if(renderPlatform&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pD3D11Buffer);
	SAFE_RELEASE(m_pD3D11Buffer);	
	D3D11_BUFFER_DESC cb_desc= { 0 };
	cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags			= 0;
	cb_desc.ByteWidth			=byteWidth;
	cb_desc.StructureByteStride = 0;
//	ID3D11Device *device		=renderPlatform->AsD3D11Device();
	D3D11_USAGE usage			=D3D11_USAGE_DYNAMIC;
	if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
		usage=D3D11_USAGE_DEFAULT;
	cb_desc.Usage				= usage;
#if (SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT)
	num_this_frame = 0;
	if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
	{
		UINT numBuffers			=m_nContexts*m_nBuffering;
		SIMUL_ASSERT( cb_desc.Usage == D3D11_USAGE_DEFAULT );
		byteWidth				=cb_desc.ByteWidth;

		#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		// Force cache line alignment and packing to avoid cache flushing on Unmap in RoundRobinBuffers
		byteWidth				=static_cast<UINT>( NextMultiple( byteWidth, SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE ) );
		#endif
	
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
		VirtualFree( m_pPlacementBuffer, 0, MEM_RELEASE );
#endif
		m_pPlacementBuffer = static_cast<BYTE*>( VirtualAlloc( nullptr, numBuffers * byteWidth, 
			MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, 
			PAGE_WRITECOMBINE | PAGE_READWRITE ));

		#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		SIMUL_ASSERT( ( BytePtrToUint64( m_pPlacementBuffer ) & ( SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE - 1 ) ) == 0 );
		#endif

		V_CHECK( ((ID3D11DeviceX*)renderPlatform->AsD3D11Device())->CreatePlacementBuffer( &cb_desc, nullptr, &m_pD3D11Buffer ) );
		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(m_pD3D11Buffer,numBuffers*byteWidth,"PlatformConstantBuffer");
	}
	else
#endif
	{
		if(addr==nullptr)
		{
			renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc,nullptr, &m_pD3D11Buffer);
			if(renderPlatform&&renderPlatform->GetMemoryInterface())
				renderPlatform->GetMemoryInterface()->TrackVideoMemory(m_pD3D11Buffer,cb_desc.ByteWidth,"PlatformConstantBuffer");
		}
		else
		{
			D3D11_SUBRESOURCE_DATA cb_init_data;
			cb_init_data.pSysMem			= addr;
			cb_init_data.SysMemPitch		= 0;
			cb_init_data.SysMemSlicePitch	= 0;
			renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc,&cb_init_data, &m_pD3D11Buffer);
		}
	}
}

void PlatformConstantBuffer::SetNumBuffers(crossplatform::RenderPlatform *r, UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering )
{
#if  (SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT)
	if(((dx11::RenderPlatform*)r)->UsesFastSemantics())
	{
		m_nContexts = nContexts;
		m_nFramesBuffering=nFramesBuffering;
		m_nBuffering = nMapsPerFrame * nFramesBuffering+1;
		m_index.resize( nContexts , 0 );
	}
	else
#endif
	{
		UNREFERENCED_PARAMETER(nContexts);
		UNREFERENCED_PARAMETER(nMapsPerFrame);
		UNREFERENCED_PARAMETER(nFramesBuffering);
		m_nContexts = 1;
		m_nBuffering = 1;
		m_nFramesBuffering=1;
	}
	CreateBuffers( r, nullptr);
}

ID3D11Buffer *PlatformConstantBuffer::asD3D11Buffer()
{
	return m_pD3D11Buffer;
}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r,size_t size,void *addr)
{
	InvalidateDeviceObjects();
	if(!r)
		return;
	byteWidth= (UINT)(PAD16(size));
	SetNumBuffers( r,1, 64, 2 );
	last_placement=nullptr;
}

//! Find the constant buffer in the given effect, and link to it.
void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *,const char *,int)
{
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	if(renderPlatform&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pD3D11Buffer);
	SAFE_RELEASE(m_pD3D11Buffer);
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(m_pPlacementBuffer)
		VirtualFree( m_pPlacementBuffer, 0, MEM_RELEASE );
#endif
	last_placement=nullptr;
}

void  PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr)
{
	if(!m_pD3D11Buffer)
	{
		SIMUL_CERR<<"Attempting to apply an uninitialized Constant Buffer"<<std::endl;
		return;
	}
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	D3D11_MAPPED_SUBRESOURCE mapped_res;
#if  SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
	{
		if(num_this_frame>=m_nBuffering)
		{
			// Too many, need a bigger buffer.
			SIMUL_CERR<<"Need a bigger buffer for PlatformConstantBuffer: resizing.\n";
			resize=true;
		}
		if(resize)
		{
			SetNumBuffers(deviceContext.renderPlatform,m_nContexts,m_nBuffering*2,m_nFramesBuffering);
			resize=false;
		}
		UINT &buffer_index = m_index[ 0 ];
		last_placement =( &m_pPlacementBuffer[buffer_index* byteWidth ] );
		memcpy(last_placement,addr,size);
		buffer_index++;
		if ( buffer_index >= m_nBuffering )
		{
			buffer_index = 0;
		}
		if(framenumber!=deviceContext.frame_number)
		{
			num_this_frame=0;
		}
		num_this_frame++;
		framenumber=deviceContext.frame_number;
		#ifndef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
			#error use cache line aligned buffers, otherwise an explicit flush for fast semantics is required here
		#endif
	}
	else
#endif
	{
		last_placement=nullptr;
		D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
		if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			map_type=D3D11_MAP_WRITE;
		unsigned flags = ((dx11::RenderPlatform*)deviceContext.renderPlatform)->GetMapFlags();
		HRESULT hr = pContext->Map(m_pD3D11Buffer, 0, map_type, flags, &mapped_res);
		V_CHECK(hr);
		if(mapped_res.pData)
		memcpy(mapped_res.pData,addr,size);
		pContext->Unmap(m_pD3D11Buffer, 0);
	}
}

void *PlatformConstantBuffer::GetBaseAddr()
{
#if  SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(last_placement)
		return last_placement;
	return nullptr;
#else
	return nullptr;
 #endif
}



void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{
}
