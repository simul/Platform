#ifdef _XBOX_ONE
#include "ESRAMTexture.h"
#include <xg.h>
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"

#include <string>

using namespace simul;
using namespace dx11;

template< typename t_A, typename t_B >
t_A RoundUpToNextMultiple( const t_A& a, const t_B& b )
{
    return ( ( a - 1 ) / b + 1 ) * b;
}

XG_FORMAT ToXgFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return XG_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return XG_FORMAT_R16G16B16A16_FLOAT;
	case RGBA_32_FLOAT:
		return XG_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return XG_FORMAT_R32G32B32_FLOAT;
	case RG_32_FLOAT:
		return XG_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return XG_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return XG_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return XG_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_SNORM:
		return XG_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return XG_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return XG_FORMAT_R8_SNORM;
	case R_32_UINT:
		return XG_FORMAT_R32_UINT;
	case RG_32_UINT:
		return XG_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return XG_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return XG_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return XG_FORMAT_D32_FLOAT;
	default:
		return XG_FORMAT_UNKNOWN;
	};
}
static UINT64 iCurrentESRAMOffset = 0; // We allow this to grow beyond ESRAM_SIZE

ESRAMTexture::ESRAMTexture()
{
}


ESRAMTexture::~ESRAMTexture()
{
}

void dx11::Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform *renderPlatform
												 ,int w,int l
												 ,crossplatform::PixelFormat pixelFormat
												 ,bool computable,bool rendertarget,bool depthstencil
												 ,int num_samples,int aa_quality)
{
	format=(DXGI_FORMAT)dx11::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT texture2dFormat=format;
	DXGI_FORMAT srvFormat=format;
	if(texture2dFormat==DXGI_FORMAT_D32_FLOAT)
	{
		texture2dFormat	=DXGI_FORMAT_R32_TYPELESS;
		srvFormat		=DXGI_FORMAT_R32_FLOAT;
	}
	if(texture2dFormat==DXGI_FORMAT_D16_UNORM)
	{
		texture2dFormat	=DXGI_FORMAT_R16_TYPELESS;
		srvFormat		=DXGI_FORMAT_R16_UNORM;
	}
	dim=2;
	ID3D11Device *pd3dDevice=renderPlatform->AsD3D11Device();
	D3D11_TEXTURE2D_DESC textureDesc;
	bool ok=true;
	if(texture)
	{
		ID3D11Texture2D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D11Texture2D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=w||textureDesc.Height!=l||textureDesc.Format!=texture2dFormat)
				ok=false;
			if(computable!=((textureDesc.BindFlags&D3D11_BIND_UNORDERED_ACCESS)==D3D11_BIND_UNORDERED_ACCESS))
				ok=false;
			if(rendertarget!=((textureDesc.BindFlags&D3D11_BIND_RENDER_TARGET)==D3D11_BIND_RENDER_TARGET))
				ok=false;
		}
		SAFE_RELEASE(ppd);
	}
	else
		ok=false; 
	if(!ok)
	{
		InvalidateDeviceObjects();

		unsigned int numQualityLevels=0;
		while(numQualityLevels==0&&num_samples>1)
		{
			V_CHECK(renderPlatform->AsD3D11Device()->CheckMultisampleQualityLevels(
				texture2dFormat,
				num_samples,
				&numQualityLevels	));
			//if(aa_quality>=numQualityLevels)
			//	aa_quality=numQualityLevels-1;
			if(numQualityLevels==0)
				num_samples/=2;
		};

		memset(&textureDesc,0,sizeof(textureDesc));
		textureDesc.Width					=width=w;
		textureDesc.Height					=length=l;
		depth								=1;
		textureDesc.Format					=texture2dFormat;
		textureDesc.MipLevels				=1;
		textureDesc.ArraySize				=1;
		textureDesc.Usage					=(computable||rendertarget||depthstencil)?D3D11_USAGE_DEFAULT:D3D11_USAGE_DYNAMIC;
		textureDesc.BindFlags				=D3D11_BIND_SHADER_RESOURCE|(computable?D3D11_BIND_UNORDERED_ACCESS:0)|(rendertarget?D3D11_BIND_RENDER_TARGET:0)|(depthstencil?D3D11_BIND_DEPTH_STENCIL:0);
		textureDesc.CPUAccessFlags			=(computable||rendertarget||depthstencil)?0:D3D11_CPU_ACCESS_WRITE;
		textureDesc.MiscFlags				=rendertarget?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;
		textureDesc.SampleDesc.Count		=num_samples;
		textureDesc.SampleDesc.Quality		=aa_quality;
		
		if(!esram)
		{
			V_CHECK(pd3dDevice->CreateTexture2D(&textureDesc,0,(ID3D11Texture2D**)(&texture)));
		}
		else
		{
			XG_FORMAT xgFormat					=ToXgFormat(pixelFormat);
			// Figure out size and alignment for ESRAM surface
			XG_TILE_MODE tileMode;
			XG_TILE_MODE StencilTileMode;
			if(!depthstencil)
			{
				tileMode= XGComputeOptimalTileMode( XG_RESOURCE_DIMENSION_TEXTURE2D, 
					xgFormat, 
					w, 
					l, 
					1, 
					1, 
					XG_BIND_RENDER_TARGET );
			}
			else
			{
				XGComputeOptimalDepthStencilTileModes( xgFormat, 
					w, 
					l,
					1, 
					1, 
					TRUE,	
					&tileMode, 
					&StencilTileMode );
					SIMUL_ASSERT( tileMode == StencilTileMode ); // We have no way to mix these currently
			}
			struct XG_TEXTURE2D_DESC eSRAMXGDesc = 
			{
				 w,							// UINT Width;
				 l,							// UINT Height;
				 1,							// UINT MipLevels;
				 1,							// UINT ArraySize;
				 xgFormat,					// XG_FORMAT Format;
				 {							// XG_SAMPLE_DESC SampleDesc;
					num_samples,						// UINT Count;
					(UINT) D3D11_STANDARD_MULTISAMPLE_PATTERN,	// UINT Quality;
				 }, 
				 XG_USAGE_DEFAULT,			// XG_USAGE Usage;
				 depthstencil?
					XG_BIND_DEPTH_STENCIL
					:XG_BIND_RENDER_TARGET,	// UINT BindFlags;
				 0,							// UINT CPUAccessFlags;
				 0,							// UINT MiscFlags;
				 0,							// UINT ESRAMOffsetBytes; Unused
				 0,							// UINT ESRAMUsageBytes; Unused
				 tileMode,					// XG_TILE_MODE TileMode;
				 0,							// UINT Pitch;
			};
		
			XGTextureAddressComputer* addressComputer;
			XGCreateTexture2DComputer( &eSRAMXGDesc, &addressComputer );
			XG_RESOURCE_LAYOUT resourceLayout;
			addressComputer->GetResourceLayout( &resourceLayout );
			RoundUpToNextMultiple( iCurrentESRAMOffset, resourceLayout.BaseAlignmentBytes );
		
			UINT iColorESRAMOffset = (UINT) iCurrentESRAMOffset;	 
			iCurrentESRAMOffset += resourceLayout.SizeBytes;	// If this grows beyond ESRAM_SIZE, we will overflow to DRAM
		
			// Now create the ESRAM resources
			D3D11_TEXTURE2D_DESC eSRAMDesc = textureDesc;
		
			const UINT64 ESRAM_SIZE = 32 * 1024 * 1024;
			if( iCurrentESRAMOffset <= ESRAM_SIZE )
			{
				 // The MISC_ESRAM_RESIDENT flag is only permitted for automatic creation
				 eSRAMDesc.MiscFlags |= D3D11X_RESOURCE_MISC_ESRAM_RESIDENT;
				 eSRAMDesc.ESRAMOffsetBytes = iColorESRAMOffset;
		
				 // Standard creation
				 V_CHECK( pd3dDevice->CreateTexture2D(&eSRAMDesc, nullptr, (ID3D11Texture2D**)(&texture) ) );
			}
			else
			{
	#define USE_ALLOCATE_TITLE_PHYSICAL_PAGES 1
	#define USE_4MB_PAGES 1
	#if USE_4MB_PAGES
				// Choose page size --- these must be consistent with each other
				const UINT64 iPageSize = 4 * 1024 * 1024;
				const ULONG lPageType = MEM_4MB_PAGES;
				const UINT iESRAMPageType = D3D11_MAP_ESRAM_4MB_PAGES;
	#else
				// Choose page size --- these must be consistent with each other
				const UINT64 iPageSize = 64 * 1024;
				const ULONG lPageType = MEM_LARGE_PAGES;
				const UINT iESRAMPageType = D3D11_MAP_ESRAM_LARGE_PAGES;
	#endif
				const UINT iMinPageSize = 64 * 1024;
				static_assert( iPageSize % iMinPageSize == 0, "Unexpected alignment mismatch" );
			
				// PAGE_NOACCESS not working right now --- possible bug?
				const DWORD dwProtect = PAGE_READONLY; 
			
				// Reserve virtual range, but do not commit
				void *CpuVirtualAddress = VirtualAlloc( nullptr, 
					 iCurrentESRAMOffset, 
					 MEM_GRAPHICS | lPageType | MEM_RESERVE, 
					 dwProtect );
				SIMUL_ASSERT( CpuVirtualAddress != nullptr );
			
				// Map ESRAM pages
				UINT iNumESRAMPages = (UINT) RoundUpToNextMultiple( std::min( iCurrentESRAMOffset, ESRAM_SIZE ), iPageSize ) / iPageSize;
				UINT ESRAMPageList[ ESRAM_SIZE / iPageSize ];
				for( UINT i = 0; i < _countof( ESRAMPageList ); ++i ) ESRAMPageList[i] = i;
				V_CHECK( D3DMapEsramMemory( iESRAMPageType, 
					 CpuVirtualAddress, 
					 iNumESRAMPages, 
					 ESRAMPageList ) );
			
				// This check is redundant here, but good for sanity check
				if( iCurrentESRAMOffset > ESRAM_SIZE )
				{
					 // Commit physical pages for ESRAM overflow into DRAM.	Can either 
					 //	1) Preallocate these pages with AllocateTitlePhysicalPages, and commit them with MapTitlePhysicalPages
					 //	2) Let the system choose the physical pages to commit
	#if USE_ALLOCATE_TITLE_PHYSICAL_PAGES
					// AllocateTitlePhysicalPages uses the min page size of 64 KB, regardless of the requested GPU page size.
					// This allows you to change the page size dynamically after allocation.
					ULONG_PTR iNumDRAMPages = RoundUpToNextMultiple( iCurrentESRAMOffset - ESRAM_SIZE, iPageSize ) / iMinPageSize;
					ULONG_PTR* DRAMPageArray = (ULONG_PTR*) alloca( iNumDRAMPages * sizeof( DRAMPageArray[0] ) );
					SIMUL_ASSERT( AllocateTitlePhysicalPages( GetCurrentProcess(), 
						 lPageType,	
						 &iNumDRAMPages, 
						 DRAMPageArray ) );
				
					// Create the appropriate virtual to physical mapping for DRAM
					SIMUL_ASSERT( nullptr != MapTitlePhysicalPages( reinterpret_cast<BYTE*>( CpuVirtualAddress ) + ESRAM_SIZE, 
						 iNumDRAMPages, 
						 MEM_GRAPHICS | lPageType, 
						 dwProtect, 
						 DRAMPageArray ) );
	#else
					void *ConfirmCpuVirtualAddress = VirtualAlloc( reinterpret_cast<BYTE*>( CpuVirtualAddress ) + ESRAM_SIZE, 
						 iCurrentESRAMOffset - ESRAM_SIZE, 
						 MEM_GRAPHICS | lPageType | MEM_COMMIT, 
						 dwProtect );
					XSF_ASSERT( ConfirmCpuVirtualAddress == reinterpret_cast<BYTE*>( CpuVirtualAddress ) + ESRAM_SIZE );
	#endif
				}
				ID3DXboxPerformanceDevice* pPerfDev;
				V_CHECK( pd3dDevice->QueryInterface( __uuidof(pPerfDev), reinterpret_cast<void**>(&pPerfDev) ) );
			
				V_CHECK( pPerfDev->CreatePlacementTexture2D( &eSRAMDesc, 
					 tileMode, 
					 0, 
					 reinterpret_cast<BYTE*>(CpuVirtualAddress) + iColorESRAMOffset, 
					  (ID3D11Texture2D**)(&texture) ) );
			}
		}
		
		SetDebugObjectName(texture,"dx11::ESRAMTexture::ensureTexture2DSizeAndFormat");
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv_desc.Format						=srvFormat;
		srv_desc.ViewDimension				=num_samples>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels		=1;
		srv_desc.Texture2D.MostDetailedMip	=0;
		V_CHECK(pd3dDevice->CreateShaderResourceView(texture,&srv_desc,&shaderResourceView));
		SetDebugObjectName(shaderResourceView,"dx11::Texture::ensureTexture2DSizeAndFormat shaderResourceView");

	}
	if(computable&&(!unorderedAccessView||!ok))
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		uav_desc.Format						=texture2dFormat;
		uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice			=0;

		SAFE_RELEASE(unorderedAccessView);
		V_CHECK(pd3dDevice->CreateUnorderedAccessView(texture,&uav_desc,&unorderedAccessView));
		SetDebugObjectName(unorderedAccessView,"dx11::Texture::ensureTexture2DSizeAndFormat unorderedAccessView");
	}
	if(rendertarget&&(!renderTargetView||!ok))
	{
		// Setup the description of the render target view.
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format				=texture2dFormat;
		renderTargetViewDesc.ViewDimension		=num_samples>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice	=0;
		// Create the render target in DX11:
		SAFE_RELEASE(renderTargetView);
		V_CHECK(pd3dDevice->CreateRenderTargetView(texture,&renderTargetViewDesc,&renderTargetView));
		SetDebugObjectName(renderTargetView,"dx11::Texture::ensureTexture2DSizeAndFormat renderTargetView");
	}
	if(depthstencil&&(!depthStencilView||!ok))
	{
		D3D11_TEX2D_DSV dsv;
		dsv.MipSlice=0;
		D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc;
		depthDesc.ViewDimension		=num_samples>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
		depthDesc.Format			=format;
		depthDesc.Flags				=0;
		depthDesc.Texture2D			=dsv;
		V_CHECK(pd3dDevice->CreateDepthStencilView(texture,&depthDesc,&depthStencilView));
	}
}
#endif