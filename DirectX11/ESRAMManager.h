#ifdef _XBOX_ONE
#include "ESRAMTexture.h"
#include <vector>
namespace simul
{
	namespace dx11
	{
		/*!
			To create a texture:
				m_esramManager->Create( pDevice, spDepthTexDesc, m_esramDepthTexture );
			To prefetch it for later use.
				m_esramManager->Prefetch( pDevice, m_spDetailTexture, m_esramDetailTexture );
			To ensure that the texture is ready
				m_esramManager->InsertGPUWait( m_esramDetailTexture.m_esramResource );
			When we no longer need it
				m_esramManager->Discard( m_esramScannerVB );
			To ensure that it will be usable for shaders
				m_esramManager->Writeback( m_esramRenderTexture, m_spRenderTexture.Get() );
			At the end of each frame:
				m_esramManager->GarbageCollect();
		*/
		class ESRAMManager
		{
		public:
		public:
			ESRAMManager(ID3D11Device* const pDevice );

			bool IsEnabled() const;
			// Create a resource in ESRAM without any initial contents
			void Create(D3D11_BUFFER_DESC desc,  ESRAMBuffer& esramDest );
			void Create(D3D11_TEXTURE2D_DESC desc,  ESRAMTextureData& esramDest );

			// Load a DRAM resource into ESRAM
			void Prefetch(ID3D11Buffer* const pDRAMSource,  ESRAMBuffer& esramDest );
			void Prefetch(ID3D11Texture2D* const pDRAMSource,  ESRAMTextureData& esramDest );

			// Copy an ESRAM resource into DRAM
			void Writeback(ESRAMBuffer& esramSource,ID3D11Buffer* const pDRAMDest );
			void Writeback(ESRAMTextureData& esramSource,ID3D11Texture2D* const pDRAMDest );

			// Mark an ESRAM resource as no longer in use, so its memory can be reclaimed
			void Discard(ESRAMBuffer& esramBuffer );
			void Discard(ESRAMTextureData& esramTexture );

			// Free memory belonging to discarded resources
			void GarbageCollect();

			// Tell the GPU to wait on a DMA operation involving the given resource. You must do this after performing
			//  a Prefetch or a Writeback, before accessing the resource on the GPU.
			void InsertGPUWait(ESRAMResource const & esramResource );

		private:
			void DiscardInternal(ESRAMResource& esramResource );

			// We allocate ESRAM in 4K blocks, using a simple first-fit freelist allocator
			void Allocate( UINT numBytes, UINT alignment,ESRAMAllocation& alloc );
			void Free(ESRAMAllocation& alloc );

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
			ID3D11DeviceX* pDevice;
			ID3D11DeviceContextX *m_spImmediateContext;
			ID3D11DmaEngineContextX *m_spDmaContext;

			// Constants
			static const UINT BLOCK_SIZE = 4096; // The granularity of ESRAM allocations
			static const USHORT INVALID_BLOCK = (USHORT)-1; 
			static const UINT INVALID_PTR = (UINT)-1; 
			static const UINT64 INVALID_FENCE = 0;

		};
	}
}
#endif