//--------------------------------------------------------------------------------------
// ESRAMManager.cpp
//
// Manages ESRAM
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "ESRAMManager.h"
#include "DirectXTex.h" // For IsTypeless()

namespace
{
    // Align an ESRAM pointer to the given alignment value
    ESRAMManager::ESRAMPtr Align(ESRAMManager::ESRAMPtr val, UINT alignment)
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


//--------------------------------------------------------------------------------------
// Name: ESRAMManager
// Desc: Class constructor
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
ESRAMManager::ESRAMManager( XSF::D3DDevice* const pDevice, XSF::D3DDeviceContext* const pImmediateContext )
{
    m_spImmediateContext.Attach( pImmediateContext );
	pImmediateContext->AddRef();

    // Create DMA engine context
    D3D11_DMA_ENGINE_CONTEXT_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_1;
    desc.RingBufferSizeBytes = 64 * 1024;   // NOTE: Currently if you overflow the ring buffer you'll hang the GPU
    XSF_ERROR_IF_FAILED( pDevice->CreateDmaEngineContext( &desc, m_spDmaContext.ReleaseAndGetAddressOf() ) );

    // Start with one free space containing the whole of ESRAM
    m_freeSpaces.emplace_back( static_cast<USHORT>(0), static_cast<USHORT>((32 * 1024 * 1024) / BLOCK_SIZE ));
}


//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Create a buffer in ESRAM without any initial contents
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Create( ID3D11Device* const pDevice, D3D11_BUFFER_DESC desc, ESRAMBuffer& esramDest )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Create" );

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
    XSF_ERROR_IF_FAILED( XGComputeBufferLayout( &xgDesc, &layout ) );

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
        XSF_ERROR_IF_FAILED( E_FAIL );
        return;
    }

    // Create the buffer resource in ESRAM
    desc.ESRAMOffsetBytes = esramResource.m_allocation.m_esramPtr;
    desc.ESRAMUsageBytes = 0;
    XSF_ERROR_IF_FAILED( pDevice->CreateBuffer( &desc, nullptr, esramDest.m_spBuffer.ReleaseAndGetAddressOf() ) );
}


//--------------------------------------------------------------------------------------
// Name: Create
// Desc: Create a texture in ESRAM without any initial contents
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Create( ID3D11Device* const pDevice, D3D11_TEXTURE2D_DESC desc, ESRAMTexture& esramDest )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Create" );
    if( IsTypeless( desc.Format ) )
    {
        XSF_ERROR_MESSAGE( "Typeless formats are not supported because we don't know what typed format to pass when creating views." );
    }

    desc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;

    // Calculate the size and alignment required to create the texture in ESRAM
    XG_TEXTURE2D_DESC texDesc;
    ZeroMemory( &texDesc, sizeof(texDesc) );
    texDesc.Width = desc.Width;
    texDesc.Height = desc.Height;
    texDesc.MipLevels = desc.MipLevels;
    texDesc.ArraySize = desc.ArraySize;
    texDesc.Format = (XG_FORMAT)desc.Format;
    texDesc.SampleDesc.Count = desc.SampleDesc.Count;
    texDesc.SampleDesc.Quality = desc.SampleDesc.Quality;
    texDesc.Usage = (XG_USAGE)desc.Usage;
    texDesc.BindFlags = (XG_BIND_FLAG)desc.BindFlags;
    texDesc.CPUAccessFlags = desc.CPUAccessFlags;
    texDesc.MiscFlags = desc.MiscFlags;
    texDesc.ESRAMOffsetBytes = 0;
    texDesc.ESRAMUsageBytes = 0;
    texDesc.TileMode = XGComputeOptimalTileMode( XG_RESOURCE_DIMENSION_TEXTURE2D, texDesc.Format, texDesc.Width, texDesc.Height, texDesc.ArraySize, texDesc.SampleDesc.Count, texDesc.BindFlags );
    texDesc.Pitch = 0;

    ComPtr<XGTextureAddressComputer> computer = nullptr;
    XSF_ERROR_IF_FAILED( XGCreateTexture2DComputer( &texDesc, computer.ReleaseAndGetAddressOf() ) );

    XG_RESOURCE_LAYOUT layout;
    XSF_ERROR_IF_FAILED( computer->GetResourceLayout( &layout ) );

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
        XSF_ERROR_IF_FAILED( E_FAIL );
        return;
    }

    // Create the texture resource in ESRAM
    desc.ESRAMOffsetBytes = esramResource.m_allocation.m_esramPtr;
    desc.ESRAMUsageBytes = 0;
    XSF_ERROR_IF_FAILED( pDevice->CreateTexture2D( &desc, nullptr, esramDest.m_spTexture.ReleaseAndGetAddressOf() ) );

    // Create the appropriate Views for this texture
    if( desc.BindFlags & D3D11_BIND_SHADER_RESOURCE )
    {
        XSF_ERROR_IF_FAILED( pDevice->CreateShaderResourceView( esramDest.m_spTexture.Get(), nullptr, esramDest.m_spSRV.ReleaseAndGetAddressOf() ) );
    }
    if( desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS )
    {
        XSF_ERROR_IF_FAILED( pDevice->CreateUnorderedAccessView( esramDest.m_spTexture.Get(), nullptr, esramDest.m_spUAV.ReleaseAndGetAddressOf() ) );
    }
    if( desc.BindFlags & D3D11_BIND_RENDER_TARGET )
    {
        XSF_ERROR_IF_FAILED( pDevice->CreateRenderTargetView( esramDest.m_spTexture.Get(), nullptr, esramDest.m_spRTV.ReleaseAndGetAddressOf() ) );
    }
    if( desc.BindFlags & D3D11_BIND_DEPTH_STENCIL )
    {
        XSF_ERROR_IF_FAILED( pDevice->CreateDepthStencilView( esramDest.m_spTexture.Get(), nullptr, esramDest.m_spDSV.ReleaseAndGetAddressOf() ) );
    }
}

//--------------------------------------------------------------------------------------
// Name: Prefetch
// Desc: Load a DRAM buffer into ESRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Prefetch( ID3D11Device* const pDevice, ID3D11Buffer* const pDRAMSource, ESRAMBuffer& esramDest )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Prefetch" );

    D3D11_BUFFER_DESC desc;
    pDRAMSource->GetDesc( &desc );

    Create(pDevice, desc, esramDest);

    // Prefetch should be used for resources not written to by the GPU, so we don't need to sync 
    //  with the GPU like we do in Writeback
    m_spDmaContext->CopyResource( esramDest.m_spBuffer.Get(), pDRAMSource, 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramDest.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Prefetch
// Desc: Load a DRAM texture into ESRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Prefetch( ID3D11Device* const pDevice, ID3D11Texture2D* const pDRAMSource, ESRAMTexture& esramDest )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Prefetch" );

    D3D11_TEXTURE2D_DESC desc;
    pDRAMSource->GetDesc( &desc );

    Create(pDevice, desc, esramDest);

    // Prefetch should be used for resources not written to by the GPU, so we don't need to sync 
    //  with the GPU like we do in Writeback
    m_spDmaContext->CopyResource( esramDest.m_spTexture.Get(), pDRAMSource, 0 );

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
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Writeback" );

    // Writeback is typically used for resources that are written by the GPU. Thus we need to ensure that 
    //  the GPU is done using this resource before we start the DMA operation. We insert a GPU fence and wait
    //  on it with the DMA engine. Once we get an API to access the internal write fence for a given resource,  
    //  we can wait on that instead.
    m_spImmediateContext->FlushGpuCaches( esramSource.m_spBuffer.Get() );
    UINT64 fence = m_spImmediateContext->InsertFence( 0 );
    m_spDmaContext->InsertWaitOnFence( 0, fence );

    m_spDmaContext->CopyResource( pDRAMDest, esramSource.m_spBuffer.Get(), 0 );

    // Insert a fence after the copy and kickoff the DMA engine
    esramSource.m_esramResource.m_fence = m_spDmaContext->InsertFence( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Writeback
// Desc: Copy an ESRAM texture into DRAM
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Writeback( ESRAMTexture& esramSource, ID3D11Texture2D* pDRAMDest )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Writeback" );

    // Writeback is typically used for resources that are written by the GPU. Thus we need to ensure that 
    //  the GPU is done using this resource before we start the DMA operation. We insert a GPU fence and wait
    //  on it with the DMA engine. Once we get an API to access the internal write fence for a given resource,  
    //  we can wait on that instead.
    m_spImmediateContext->FlushGpuCaches( esramSource.m_spTexture.Get() );
    UINT64 fence = m_spImmediateContext->InsertFence( 0 );
    m_spDmaContext->InsertWaitOnFence( 0, fence );

    m_spDmaContext->CopyResource( pDRAMDest, esramSource.m_spTexture.Get(), 0 );

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
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Discard" );

    // Flush the GPU caches to ensure the result of any GPU writes takes effect before we reclaim
    //  the memory for another purpose
    m_spImmediateContext->FlushGpuCaches( esramBuffer.m_spBuffer.Get() );

    DiscardInternal( esramBuffer.m_esramResource );
}

_Use_decl_annotations_
void ESRAMManager::Discard( ESRAMTexture& esramTexture )
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Discard" );

    // Flush the GPU caches to ensure the result of any GPU writes takes effect before we reclaim
    //  the memory for another purpose
    m_spImmediateContext->FlushGpuCaches( esramTexture.m_spTexture.Get() );

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
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager GarbageCollect" );

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
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager InsertGPUWait" );

    m_spImmediateContext->InsertWaitOnFence( D3D11_INSERT_FENCE_NO_KICKOFF, esramResource.m_fence );
}


//--------------------------------------------------------------------------------------
// Name: Allocate
// Desc: Allocate memory in ESRAM using a simple first-fit freelist allocator
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void ESRAMManager::Allocate(UINT numBytes, UINT alignment, ESRAMAllocation& alloc)
{
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Allocate" );

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
    XSFScopedNamedEvent( m_spImmediateContext.Get(), XSF_COLOR_RENDER, L"ESRAMManager Free" );

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
