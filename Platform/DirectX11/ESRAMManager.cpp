#ifdef _XBOX_ONE
#include "ESRAMManager.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Base/RuntimeError.h"
#include <xg.h>
using namespace simul;
using namespace dx11;

namespace
{
    // Align an ESRAM pointer to the given alignment value
    ESRAMPtr Align(ESRAMPtr val, UINT alignment)
    {
        if(val % alignment != 0)
        {
            return val + (alignment - val % alignment);
        }
        else
        {
            return val;
        }        
    }
}

_Use_decl_annotations_
ESRAMManager::ESRAMManager( ID3D11Device* const dev )
	:	pDevice(nullptr)
		,m_spImmediateContext(nullptr)
		,m_spDmaContext(nullptr)
{
	pDevice=(ID3D11DeviceX*)dev;
	ID3D11DeviceContext *imm=NULL;
	pDevice->GetImmediateContext(&imm);
	m_spImmediateContext=(ID3D11DeviceContextX*)imm;
    // Create DMA engine context
    D3D11_DMA_ENGINE_CONTEXT_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_1;
    desc.RingBufferSizeBytes = 64 * 1024;   // NOTE: Currently if you overflow the ring buffer you'll hang the GPU
    HRESULT hr=( pDevice->CreateDmaEngineContext( &desc,&m_spDmaContext ) );

    // Start with one free space containing the whole of ESRAM
    m_freeSpaces.emplace_back( static_cast<USHORT>(0), static_cast<USHORT>((32 * 1024 * 1024) / BLOCK_SIZE ));

	if(hr!=S_OK)
	{
		SIMUL_CERR<<"Failed to created Dma Engine Context - ESRAM will not be used by Simul."<<std::endl;
	}
	
}

bool ESRAMManager::IsEnabled() const
{
	return (m_spDmaContext!=nullptr);
}


//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Create a buffer in ESRAM without any initial contents
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Create(  D3D11_BUFFER_DESC desc, ESRAMBuffer& esramDest )
{
    desc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;

    // Calculate the size and alignment required to create the buffer in ESRAM
    XG_BUFFER_DESC xgDesc;
    ZeroMemory( &xgDesc, sizeof(xgDesc) );
    xgDesc.ByteWidth = desc.ByteWidth;
    xgDesc.Usage = (XG_USAGE)desc.Usage;
    xgDesc.BindFlags = desc.BindFlags;
    xgDesc.CPUAccessFlags = desc.CPUAccessFlags;
    xgDesc.MiscFlags = desc.MiscFlags;
    xgDesc.StructureByteStride = desc.StructureByteStride;
    xgDesc.ESRAMOffsetBytes = 0;
    xgDesc.ESRAMUsageBytes = 0;

    XG_RESOURCE_LAYOUT layout;
    V_CHECK( XGComputeBufferLayout( &xgDesc, &layout ) );

    // Try to allocate the proper amount of space in ESRAM
    ESRAMResource& esramResource = esramDest.m_esramResource;
    Allocate( (UINT)layout.SizeBytes, (UINT)layout.BaseAlignmentBytes, esramResource.m_allocation );
    if( !esramResource.m_allocation.IsValid() )
    {
        // The allocation failed. Try freeing any discarded resources, and try again
        GarbageCollect();
        Allocate( (UINT)layout.SizeBytes, (UINT)layout.BaseAlignmentBytes, esramResource.m_allocation );
    }
    if( !esramResource.m_allocation.IsValid() )
    {
        // The allocation failed. Crash.
        V_CHECK( E_FAIL );
        return;
    }

    // Create the buffer resource in ESRAM
    desc.ESRAMOffsetBytes = esramResource.m_allocation.m_esramPtr;
    desc.ESRAMUsageBytes = 0;
    V_CHECK( pDevice->CreateBuffer( &desc, nullptr, &esramDest.m_spBuffer ) );
}


//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Create a texture in ESRAM without any initial contents
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Create(  D3D11_TEXTURE2D_DESC desc, ESRAMTextureData& esramDest )
{
    if( IsTypeless( desc.Format,false) )
    {
		if(desc.Format==DXGI_FORMAT_R32_TYPELESS)
			desc.Format	=DXGI_FORMAT_D32_FLOAT;
		else if(desc.Format==DXGI_FORMAT_R16_TYPELESS)
			desc.Format	=DXGI_FORMAT_D16_UNORM;
		else SIMUL_BREAK("Unknown typeless format");
    }
	DXGI_FORMAT mainFormat=desc.Format;
	DXGI_FORMAT srvFormat=desc.Format;
	if(mainFormat==DXGI_FORMAT_D32_FLOAT)
	{
		desc.Format		=DXGI_FORMAT_R32_TYPELESS;
		srvFormat		=DXGI_FORMAT_R32_FLOAT;
	}
	else if(mainFormat==DXGI_FORMAT_D16_UNORM)
	{
		desc.Format		=DXGI_FORMAT_R16_TYPELESS;
		srvFormat		=DXGI_FORMAT_R16_UNORM;
	}

    desc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;

    // Calculate the size and alignment required to create the texture in ESRAM
    XG_TEXTURE2D_DESC eSRAMXGDesc;
    ZeroMemory( &eSRAMXGDesc, sizeof(eSRAMXGDesc) );
    eSRAMXGDesc.Width				= desc.Width;
    eSRAMXGDesc.Height				= desc.Height;
    eSRAMXGDesc.MipLevels			= desc.MipLevels;
    eSRAMXGDesc.ArraySize			= desc.ArraySize;
    eSRAMXGDesc.Format				= (XG_FORMAT)desc.Format;
    eSRAMXGDesc.SampleDesc.Count	= desc.SampleDesc.Count;
    eSRAMXGDesc.SampleDesc.Quality	= desc.SampleDesc.Quality;
    eSRAMXGDesc.Usage				= (XG_USAGE)desc.Usage;
    eSRAMXGDesc.BindFlags			= (XG_BIND_FLAG)desc.BindFlags;
    eSRAMXGDesc.CPUAccessFlags		= desc.CPUAccessFlags;
    eSRAMXGDesc.MiscFlags			= desc.MiscFlags;
    eSRAMXGDesc.ESRAMOffsetBytes	= 0;
    eSRAMXGDesc.ESRAMUsageBytes		= 0;
    eSRAMXGDesc.TileMode			= XGComputeOptimalTileMode( XG_RESOURCE_DIMENSION_TEXTURE2D, eSRAMXGDesc.Format, eSRAMXGDesc.Width, eSRAMXGDesc.Height, eSRAMXGDesc.ArraySize, eSRAMXGDesc.SampleDesc.Count, eSRAMXGDesc.BindFlags );
    eSRAMXGDesc.Pitch				= 0;

    XGTextureAddressComputer *computer = nullptr;
    V_CHECK( XGCreateTexture2DComputer( &eSRAMXGDesc,&computer ) );

    XG_RESOURCE_LAYOUT layout;
    V_CHECK( computer->GetResourceLayout( &layout ) );

    // Try to allocate the proper amount of space in ESRAM
    ESRAMResource& esramResource = esramDest.m_esramResource;
    Allocate( (UINT)layout.SizeBytes, (UINT)layout.BaseAlignmentBytes, esramResource.m_allocation );
    if( !esramResource.m_allocation.IsValid() )
    {
        // The allocation failed. Try freeing any discarded resources, and try again
        GarbageCollect();
        Allocate( (UINT)layout.SizeBytes, (UINT)layout.BaseAlignmentBytes, esramResource.m_allocation );
    }
    if( !esramResource.m_allocation.IsValid() )
    {
        // The allocation failed. Crash.
        V_CHECK( E_FAIL );
        return;
    }
    // Create the texture resource in ESRAM
    desc.ESRAMOffsetBytes = esramResource.m_allocation.m_esramPtr;
    desc.ESRAMUsageBytes = 0;
	ID3D11Texture2D*t=NULL;
    V_CHECK( pDevice->CreateTexture2D( &desc, nullptr, &t ) );
	esramDest.m_pESRAMTexture2D=t;
    // Create the appropriate Views for this texture
    if( desc.BindFlags & D3D11_BIND_SHADER_RESOURCE )
    {
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						=srvFormat;
		srv_desc.ViewDimension				=desc.SampleDesc.Count>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels		=1;
		srv_desc.Texture2D.MostDetailedMip	=0;
        V_CHECK( pDevice->CreateShaderResourceView( t, &srv_desc, &esramDest.m_pESRAMSRV ) );
    }
    if( desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS )
    {
        V_CHECK( pDevice->CreateUnorderedAccessView( t, nullptr, &esramDest.m_pESRAMUAV ) );
    }
    if( desc.BindFlags & D3D11_BIND_RENDER_TARGET )
    {
        V_CHECK( pDevice->CreateRenderTargetView( t, nullptr, &esramDest.m_pESRAMRTV ) );
    }
    if( desc.BindFlags & D3D11_BIND_DEPTH_STENCIL )
    {
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
		ZeroMemory(&dsv_desc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
		dsv_desc.Format						=mainFormat;
		dsv_desc.Texture2D.MipSlice=0;
		dsv_desc.ViewDimension				=desc.SampleDesc.Count>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
        V_CHECK( pDevice->CreateDepthStencilView( t, &dsv_desc, &esramDest.m_pESRAMDSV ) );
    }
}

//--------------------------------------------------------------------------------------
// Name: Prefetch
// Desc: Load a DRAM buffer into ESRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Prefetch(  ID3D11Buffer* const pDRAMSource, ESRAMBuffer& esramDest )
{

    D3D11_BUFFER_DESC desc;
    pDRAMSource->GetDesc( &desc );

    Create( desc, esramDest);

    // Prefetch should be used for resources not written to by the GPU, so we don't need to sync 
    //  with the GPU like we do in Writeback
    m_spDmaContext->CopyResource( esramDest.m_spBuffer, pDRAMSource, 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramDest.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Prefetch
// Desc: Load a DRAM texture into ESRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Prefetch(  ID3D11Texture2D* const pDRAMSource, ESRAMTextureData& esramDest )
{
    D3D11_TEXTURE2D_DESC desc;
    pDRAMSource->GetDesc( &desc );

    Create( desc, esramDest);

    // Prefetch should be used for resources not written to by the GPU, so we don't need to sync 
    //  with the GPU like we do in Writeback
    m_spDmaContext->CopyResource( esramDest.m_pESRAMTexture2D, pDRAMSource, 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramDest.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}

//--------------------------------------------------------------------------------------
// Name: Writeback
// Desc: Copy an ESRAM buffer into DRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Writeback( ESRAMBuffer& esramSource, ID3D11Buffer* pDRAMDest )
{
    // Writeback is typically used for resources that are written by the GPU. Thus we need to ensure that 
    //  the GPU is done using this resource before we start the DMA operation. We insert a GPU fence and wait
    //  on it with the DMA engine. Once we get an API to access the internal write fence for a given resource,  
    //  we can wait on that instead.
    m_spImmediateContext->FlushGpuCaches( esramSource.m_spBuffer );
    UINT64 fence = m_spImmediateContext->InsertFence( 0 );
    m_spDmaContext->InsertWaitOnFence( 0, fence );

    m_spDmaContext->CopyResource( pDRAMDest, esramSource.m_spBuffer, 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramSource.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Writeback
// Desc: Copy an ESRAM texture into DRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Writeback( ESRAMTextureData& esramSource, ID3D11Texture2D* pDRAMDest )
{
	if(esramSource.m_pESRAMDSV||esramSource.m_pESRAMRTV)
		m_spImmediateContext->DecompressResource( esramSource.m_pESRAMTexture2D, 0, nullptr, esramSource.m_pESRAMTexture2D, 0, nullptr, DXGI_FORMAT_UNKNOWN, D3D11X_DECOMPRESS_ALL );
    // Writeback is typically used for resources that are written by the GPU. Thus we need to ensure that 
    //  the GPU is done using this resource before we start the DMA operation. We insert a GPU fence and wait
    //  on it with the DMA engine. Once we get an API to access the internal write fence for a given resource,  
    //  we can wait on that instead.
    m_spImmediateContext->FlushGpuCaches( esramSource.m_pESRAMTexture2D );
    UINT64 fence = m_spImmediateContext->InsertFence( 0 );
    m_spDmaContext->InsertWaitOnFence( 0, fence );

    m_spDmaContext->CopyResource( pDRAMDest, esramSource.m_pESRAMTexture2D, 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramSource.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Discard
// Desc: Mark an ESRAM resource as no longer in use, so its memory can be reclaimed
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Discard( ESRAMBuffer& esramBuffer )
{
    // Flush the GPU caches to ensure the result of any GPU writes takes effect before we reclaim
    //  the memory for another purpose
    m_spImmediateContext->FlushGpuCaches( esramBuffer.m_spBuffer );

    DiscardInternal( esramBuffer.m_esramResource );
}

_Use_decl_annotations_
void ESRAMManager::Discard( ESRAMTextureData& esramTexture )
{
    // Flush the GPU caches to ensure the result of any GPU writes takes effect before we reclaim
    //  the memory for another purpose
    m_spImmediateContext->FlushGpuCaches( esramTexture.m_pESRAMTexture2D );

    DiscardInternal( esramTexture.m_esramResource );
}

void ESRAMManager::DiscardInternal( _In_ ESRAMResource& esramResource )
{
    // Insert a fence on the GPU to mark the last time the GPU touched this resource. This fence
    //  will be used later to ensure the DMA engine doesn't trample over memory that is still in use.
    esramResource.m_fence = m_spImmediateContext->InsertFence( 0 );

    // If we waited on the fence and freed the resource memory immediately, every discard operation
    //  would block the DMA engine until the GPU caught up. That would be bad. So instead we just
    //  add the discarded resource to a list, and "garbage collect" later when we actually need 
    //  to use the free space.
    m_discardedResources.push_back( esramResource );

    esramResource.m_allocation.Invalidate();
    esramResource.m_fence = INVALID_FENCE;
}


//--------------------------------------------------------------------------------------
// Name: GarbageCollect
// Desc: Free memory belonging to discarded resources
//--------------------------------------------------------------------------------------
void ESRAMManager::GarbageCollect()
{
    for(int i = 0; i < m_discardedResources.size(); ++i)
    {
        // At this point the resource fence indicates the last time the GPU used the resource.
        //  Have the DMA engine wait on this fence to ensure that we can't copy a new resource onto
        //  the reclaimed memory until the GPU is done with it.
        m_spDmaContext->InsertWaitOnFence( D3D11_INSERT_FENCE_NO_KICKOFF, m_discardedResources[i].m_fence );
        Free( m_discardedResources[i].m_allocation );
    }
    m_discardedResources.clear();
}


//--------------------------------------------------------------------------------------
// Name: InsertGPUWait
// Desc: Tell the GPU to wait on a DMA operation involving the given resource. You must 
//  do this after performing a Prefetch or a Writeback, before accessing the resource on the GPU.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::InsertGPUWait( ESRAMResource const & esramResource )
{
    m_spImmediateContext->InsertWaitOnFence( D3D11_INSERT_FENCE_NO_KICKOFF, esramResource.m_fence );
}


//--------------------------------------------------------------------------------------
// Name: Allocate
// Desc: Allocate memory in ESRAM using a simple first-fit freelist allocator
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Allocate(UINT numBytes, UINT alignment, ESRAMAllocation& alloc)
{
    // Make sure the allocation comes back invalid if we can't fit
    alloc.Invalidate();

    // Try to find contiguous free space big enough to fit the aligned allocation
    for( auto iter = m_freeSpaces.rbegin(); iter != m_freeSpaces.rend(); ++iter )
    {
        FreeSpace& freeSpace = *iter;

        // See if we can fit the aligned allocation inside this free space
        ESRAMPtr beginPtr = freeSpace.m_beginBlock * BLOCK_SIZE;
        ESRAMPtr endPtr = freeSpace.m_endBlock * BLOCK_SIZE;
        ESRAMPtr alignedPtr = Align(beginPtr, alignment);

        if(alignedPtr + numBytes <= endPtr)
        {
            // Our allocation will fit in this free space
            alloc.m_beginBlock = freeSpace.m_beginBlock;
            alloc.m_esramPtr = alignedPtr;

            // Shrink the free space to only the bit left after the allocation
            freeSpace.m_beginBlock = (USHORT) ( Align(alignedPtr + numBytes, BLOCK_SIZE) / BLOCK_SIZE );
            alloc.m_endBlock = freeSpace.m_beginBlock;
            if(freeSpace.m_beginBlock == freeSpace.m_endBlock)
            {
                // We consumed this entire piece of free space
                m_freeSpaces.erase( (++iter).base() ); // Need to increment to get the correct iterator for erase
            }

            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Free
// Desc: Free memory in ESRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Free(ESRAMAllocation& alloc)
{
    // See if we can coalesce the freed memory with an existing free space
    FreeSpace* pCoalesced = nullptr;
    for(auto iter = m_freeSpaces.rbegin(); iter != m_freeSpaces.rend(); ++iter)
    {
        FreeSpace& freeSpace = *iter;
        if(freeSpace.m_endBlock == alloc.m_beginBlock)
        {
            // This free space comes immediately before the allocation
            if(pCoalesced == nullptr)
            {
                // Grow the free space to encompass the allocation
                freeSpace.m_endBlock = alloc.m_endBlock;
                pCoalesced = &freeSpace;
            }
            else
            {
                // We already found an area of free space that comes immediately after the allocation,
                //  and now we found an area of free space that comes immediately before the allocation.
                //  Coalesce the two free spaces.
                pCoalesced->m_beginBlock = freeSpace.m_beginBlock;
                m_freeSpaces.erase( (++iter).base() ); // Need to increment to get the correct iterator for erase
                break;
            }
        }
        else if(freeSpace.m_beginBlock == alloc.m_endBlock)
        {
            // This free space comes immediately after the allocation
            if(pCoalesced == nullptr)
            {
                // Grow the free space to encompass the allocation
                freeSpace.m_beginBlock = alloc.m_beginBlock;
                pCoalesced = &freeSpace;
            }
            else
            {
                // We already found an area of free space that comes immediately before the allocation,
                //  and now we found an area of free space that comes immediately after the allocation.
                //  Coalesce the two free spaces.
                pCoalesced->m_endBlock = freeSpace.m_endBlock;
                m_freeSpaces.erase( (++iter).base() ); // Need to increment to get the correct iterator for erase
                break;
            }
        }
    }

    if( pCoalesced == nullptr )
    {
        // We weren't able to coalesce this allocation with any existing free space, so add it to the list
        m_freeSpaces.emplace_back(alloc.m_beginBlock, alloc.m_endBlock);
    }

    alloc.Invalidate();
}
#endif