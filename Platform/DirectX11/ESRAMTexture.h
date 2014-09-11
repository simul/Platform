#pragma once
#ifdef _XBOX_ONE
#include "Simul\Platform\DirectX11\Texture.h"
#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		// Constants
		static const UINT BLOCK_SIZE = 4096; // The granularity of ESRAM allocations
		static const USHORT INVALID_BLOCK = (USHORT)-1; 
		static const UINT INVALID_PTR = (UINT)-1; 
		static const UINT64 INVALID_FENCE = 0;
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

		// A buffer allocated in ESRAM
		class ESRAMBuffer
		{
		public:
			// The buffer resource in ESRAM
			ID3D11Buffer	                   *m_spBuffer;
			ESRAMResource                       m_esramResource;
		};

		class ESRAMTexture :public Texture
		{
			// The texture resource in ESRAM : use "texture"
			//XSF::D3DTexture2DPtr                m_spTexture;
			ESRAMResource                       m_esramResource;
			// Keep track of whether the "current" version is in ESRAM or DRAM.
			bool								in_esram;
		public:
			ESRAMTexture();
			virtual ~ESRAMTexture();
			virtual void ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l
				,crossplatform::PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0) override;

			void MoveFromDRAMToESRAM();
			void MoveFromESRAMToDRAM();

			friend class ESRAMManager;
		};
	}
}
#endif