//--------------------------------------------------------------------------------------
// ESRAMManager.h
//
// Manages ESRAM
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

using Microsoft::WRL::ComPtr;

// ESRAM manager with cache-like semantics
//  Users can "Prefetch" resources into ESRAM and "Writeback" resources to DRAM
//  We allocate space in ESRAM using a simple first-fit freelist allocator
class ESRAMManager
{
public:
    typedef UINT ESRAMPtr;

    // An allocation in ESRAM
    class ESRAMAllocation
    {
    public:
        ESRAMAllocation() : m_esramPtr(INVALID_PTR), m_beginBlock(INVALID_BLOCK), m_endBlock(INVALID_BLOCK) {}

        BOOL        IsValid() const { return m_beginBlock != INVALID_BLOCK; }
        void        Invalidate() { m_esramPtr = INVALID_PTR; m_beginBlock = m_endBlock = INVALID_BLOCK; }

        ESRAMPtr    m_esramPtr;     // The start location of this allocation in ESRAM
        USHORT      m_beginBlock;   // The first 4K block of this allocation
        USHORT      m_endBlock;     // One past the end, like STL
    };

    // A resource allocated in ESRAM
    class ESRAMResource
    {
    public:
        ESRAMResource() : m_fence(INVALID_FENCE) { } 

        // The allocation in ESRAM
        ESRAMAllocation                     m_allocation;

        // A fence value for the last DMA or GPU operation that touched this resource. The 
        // resource should not be touched again until this fence has been passed.
        UINT64                              m_fence;
    };

    // A texture allocated in ESRAM
    class ESRAMTexture
    {
    public:
        // The texture resource in ESRAM
        XSF::D3DTexture2DPtr                m_spTexture;

        // Views: null if the texture does not support the given View
        XSF::D3DShaderResourceViewPtr       m_spSRV;
        XSF::D3DUnorderedAccessViewPtr      m_spUAV;
        XSF::D3DRenderTargetViewPtr         m_spRTV;
        XSF::D3DDepthStencilViewPtr         m_spDSV;

        ESRAMResource                       m_esramResource;
    };

    // A buffer allocated in ESRAM
    class ESRAMBuffer
    {
    public:
        // The buffer resource in ESRAM
        XSF::D3DBufferPtr                   m_spBuffer;

        ESRAMResource                       m_esramResource;
    };

public:
    ESRAMManager( _In_ XSF::D3DDevice* const pDevice, _In_ XSF::D3DDeviceContext* const pImmediateContext );

    // Create a resource in ESRAM without any initial contents
    void Create( _In_ ID3D11Device* const pDevice, _In_ D3D11_BUFFER_DESC desc, _Out_ ESRAMBuffer& esramDest );
    void Create( _In_ ID3D11Device* const pDevice, _In_ D3D11_TEXTURE2D_DESC desc, _Out_ ESRAMTexture& esramDest );

    // Load a DRAM resource into ESRAM
    void Prefetch( _In_ ID3D11Device* const pDevice, _In_ ID3D11Buffer* const pDRAMSource, _Out_ ESRAMBuffer& esramDest );
    void Prefetch( _In_ ID3D11Device* const pDevice, _In_ ID3D11Texture2D* const pDRAMSource, _Out_ ESRAMTexture& esramDest );

    // Copy an ESRAM resource into DRAM
    void Writeback( _In_ ESRAMBuffer& esramSource, _In_ ID3D11Buffer* const pDRAMDest );
    void Writeback( _In_ ESRAMTexture& esramSource, _In_ ID3D11Texture2D* const pDRAMDest );

    // Mark an ESRAM resource as no longer in use, so its memory can be reclaimed
    void Discard( _In_ ESRAMBuffer& esramBuffer );
    void Discard( _In_ ESRAMTexture& esramTexture );

    // Free memory belonging to discarded resources
    void GarbageCollect();

    // Tell the GPU to wait on a DMA operation involving the given resource. You must do this after performing
    //  a Prefetch or a Writeback, before accessing the resource on the GPU.
    void InsertGPUWait( _In_ ESRAMResource const & esramResource );

private:
    void DiscardInternal( _In_ ESRAMResource& esramResource );

    // We allocate ESRAM in 4K blocks, using a simple first-fit freelist allocator
    void Allocate( UINT numBytes, UINT alignment, _Out_ ESRAMAllocation& alloc );
    void Free( _In_ ESRAMAllocation& alloc );

    // An area of free space in ESRAM
    class FreeSpace
    {
    public:
        FreeSpace(USHORT beginBlock, USHORT endBlock) : m_beginBlock(beginBlock), m_endBlock(endBlock) {}

        USHORT m_beginBlock;
        USHORT m_endBlock;   // One past the end, like STL
    };

    std::vector<FreeSpace> m_freeSpaces;

    // ESRAM resources that are no longer in use but that have not yet been cleaned up
    std::vector<ESRAMResource> m_discardedResources;

    XSF::D3DDeviceContextPtr   m_spImmediateContext;
    XSF::D3DTypePtr<ID3D11DmaEngineContextX> m_spDmaContext;

    // Constants
    static const UINT BLOCK_SIZE = 4096; // The granularity of ESRAM allocations
    static const USHORT INVALID_BLOCK = (USHORT)-1; 
    static const UINT INVALID_PTR = (UINT)-1; 
    static const UINT64 INVALID_FENCE = 0;
};
