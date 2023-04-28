
#include "Texture.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include <string>
#include <algorithm>
#include <DirectXTex.h>
using namespace std::string_literals;

using namespace platform;
using namespace dx12;

SamplerState::SamplerState(crossplatform::SamplerStateDesc *d):
    mCachedDesc(*d)
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::InvalidateDeviceObjects()
{
}

Texture::Texture():
	mTextureDefault(nullptr),
	mTextureUpload(nullptr),
	mResourceState (D3D12_RESOURCE_STATE_GENERIC_READ),
	mExternalLayout(D3D12_RESOURCE_STATE_GENERIC_READ),
	mLoadedFromFile(false),
	layerShaderResourceViews12(nullptr),
	mainMipShaderResourceViews12(nullptr),
	layerMipShaderResourceViews12(nullptr),
	mipUnorderedAccessViews12(nullptr),
	layerMipUnorderedAccessViews12(nullptr),
	layerDepthStencilViews12(nullptr),
	layerMipRenderTargetViews12(nullptr),
	mNumSamples(1)
{
	// Set the pointer to an invalid value so we can perform checks
	mainShaderResourceView12.ptr	= -1;
	arrayShaderResourceView12.ptr	= -1;
	mainDepthStencilView12.ptr		= -1;
	mainRenderTargetView12.ptr		= -1;
}

Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::ClearLoadingData()
{
	for(auto &w: wicContents)
	{
		delete w.metadata;
		delete w.scratchImage;
	}
	wicContents.clear();
	ClearFileContents();
}
void Texture::ClearFileContents()
{
	for(auto &f: fileContents)
	{
		platform::core::FileLoader::GetFileLoader()->ReleaseFileContents(f.ptr);
	}
	fileContents.clear();
}

void Texture::FreeUAVTables()
{
	if (mipUnorderedAccessViews12)
	{
		delete[] mipUnorderedAccessViews12;
	}
	mipUnorderedAccessViews12 = nullptr;

	if (layerMipUnorderedAccessViews12)
	{
		for (size_t i = 0; i< layerMipUnorderedAccessViews12Size; i++)
		{
			delete[] layerMipUnorderedAccessViews12[i];
		}
		delete[] layerMipUnorderedAccessViews12;
		layerMipUnorderedAccessViews12 = nullptr;
	}
}
void Texture::InitUAVTables(int l,int m)
{

	mipUnorderedAccessViews12 = nullptr;
	if (m)
		mipUnorderedAccessViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[m];		// UAV's for whole texture at different mips.

	layerMipUnorderedAccessViews12 = nullptr;
	if (l&&m)
	{
		layerMipUnorderedAccessViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE*[l];
		for (int i = 0; i<l; i++)
		{
			layerMipUnorderedAccessViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];	// UAV's for each layer at different mips.
		}
	}
	layerMipUnorderedAccessViews12Size = l;
}

void Texture::FreeSRVTables()
{
	delete[] layerShaderResourceViews12;
	layerShaderResourceViews12 = nullptr;

	for (size_t i = 0; i < layerMipShaderResourceViews12Size; i++)
	{
		if (layerMipShaderResourceViews12)
		{
			delete[] layerMipShaderResourceViews12[i];
		}
	}
	delete[] layerMipShaderResourceViews12;
	layerMipShaderResourceViews12 = nullptr;

	if (mainMipShaderResourceViews12)
	{
		delete[] mainMipShaderResourceViews12;
	}
	mainMipShaderResourceViews12 = nullptr;
}
void Texture::InitSRVTables(int l, int m)
{
	// SRV's for each layer, including all mips
	if (l > 1)
		layerShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[l];
	else
		layerShaderResourceViews12 = nullptr;

	// SRV's for the whole texture at different mips.
	if (m > 1)
		mainMipShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[m];
	else
		mainMipShaderResourceViews12 = nullptr;

	// SRV's for each layer at different mips.
	if (l > 1 && m > 1)
	{
		layerMipShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE * [l];
		// SRV's for each layer at different mips.
		for (int i = 0; i < l; i++)
			layerMipShaderResourceViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];
	}
	else
		layerMipShaderResourceViews12 = nullptr;

	layerMipShaderResourceViews12Size = l;
}
void Texture::CreateSRVTables(int num, int m, bool cubemap, bool volume, bool msaa)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	if (!volume)
	{
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = srvFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		int totalNum = cubemap ? 6 * num : num;
		if (cubemap)
		{
			if (num <= 1)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = m;
				srvDesc.TextureCube.MostDetailedMip = 0;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srvDesc.TextureCubeArray.MipLevels = m;
				srvDesc.TextureCubeArray.MostDetailedMip = 0;
				srvDesc.TextureCubeArray.First2DArrayFace = 0;
				srvDesc.TextureCubeArray.NumCubes = num;
			}
		}
		else if (num > 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = num;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = m;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
		}
		else if (msaa)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			srvDesc.Texture2D.MipLevels = m;
			srvDesc.Texture2D.MostDetailedMip = 0;
		}

		// Calculate the size of the heap
		uint heapSize = 1 +
			(cubemap ? 1 : 0) +
			(mainMipShaderResourceViews12 ? m : 0) +
			(layerShaderResourceViews12 ? totalNum : 0) +
			(layerMipShaderResourceViews12 ? totalNum * m : 0);

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (name + " TextureSRVHeap"s).c_str(), false);

		// Create main srv
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if (cubemap)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = totalNum;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = m;
			srvDesc.Texture2DArray.MostDetailedMip = 0;

			// Cubemap (array) view
			renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
			arrayShaderResourceView12 = mTextureSrvHeap.CpuHandle();
			mTextureSrvHeap.Offset();
		}
		if (mainMipShaderResourceViews12)
		{
			if (cubemap)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = 1;
				for (int j = 0; j < m; j++)
				{
					srvDesc.TextureCube.MostDetailedMip = j;

					// View for each mip
					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
					mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
			}
			else
			{
				for (int j = 0; j < m; j++)
				{
					// TO-DO: check this
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srvDesc.Texture2DArray.ArraySize = num;
					srvDesc.Texture2DArray.FirstArraySlice = 0;
					srvDesc.Texture2DArray.MipLevels = 1;
					srvDesc.Texture2DArray.MostDetailedMip = j;

					// View for each mip
					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
					mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
			}
		}
		if (layerShaderResourceViews12 || layerMipShaderResourceViews12)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvFaceDesc = {};
			srvFaceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvFaceDesc.Format = srvDesc.Format;
			srvFaceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			if (cubemap)
				srvFaceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			for (int i = 0; i < totalNum; i++)
			{
				if (layerShaderResourceViews12)
				{
					srvFaceDesc.Texture2DArray.ArraySize = 1;
					srvFaceDesc.Texture2DArray.FirstArraySlice = i;
					srvFaceDesc.Texture2DArray.MipLevels = m;
					srvFaceDesc.Texture2DArray.MostDetailedMip = 0;

					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvFaceDesc, mTextureSrvHeap.CpuHandle());
					layerShaderResourceViews12[i] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
				if (layerMipShaderResourceViews12)
				{
					srvFaceDesc.Texture2DArray.ArraySize = 1;
					srvFaceDesc.Texture2DArray.FirstArraySlice = i;
					srvFaceDesc.Texture2DArray.MipLevels = 1;

					for (int j = 0; j < m; j++)
					{
						srvFaceDesc.Texture2DArray.MostDetailedMip = j;

						renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvFaceDesc, mTextureSrvHeap.CpuHandle());
						layerMipShaderResourceViews12[i][j] = mTextureSrvHeap.CpuHandle();
						mTextureSrvHeap.Offset();
					}
				}
			}
		}
	}
	else
	{
		// Calculate the size of the heap
		uint heapSize = 1 +
			(mainMipShaderResourceViews12 ? m : 0);

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TextureSRVHeap", false);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = RenderPlatform::TypelessToSrvFormat(dxgi_format);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = m;
		srvDesc.Texture3D.MostDetailedMip = 0;

		// Main shader view
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if (mainMipShaderResourceViews12)
		{
			for (int j = 0; j < m; j++)
			{
				srvDesc.Texture3D.MipLevels = 1;
				srvDesc.Texture3D.MostDetailedMip = j;

				// View of the mips
				renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
				mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
				mTextureSrvHeap.Offset();
			}
		}
	}
}

void Texture::FreeRTVTables()
{
	if (layerMipRenderTargetViews12)
	{
		for (size_t i = 0; i < layerMipRenderTargetViews12Size; i++)
		{
			delete[] layerMipRenderTargetViews12[i];
		}
		delete[] layerMipRenderTargetViews12;
		layerMipRenderTargetViews12 = nullptr;
	}
}
void Texture::InitRTVTables(int l,int m)
{
	layerMipRenderTargetViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE*[l];
	for (int i = 0; i<l; i++)
	{
		layerMipRenderTargetViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];
	}
	layerMipRenderTargetViews12Size = static_cast<size_t>(l);
}
void Texture::CreateRTVTables(int totalNum, int m, bool msaa)
{
	mTextureRtHeap.Restore((dx12::RenderPlatform*)renderPlatform, (totalNum * m) + (1), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "TextureRTHeap", false);
	bool array = totalNum > 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtDesc	= {};
	rtDesc.Format							= genericDxgiFormat;
	rtDesc.ViewDimension					= array ? (msaa ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
												: (msaa ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D);
	if (array)
	{
		rtDesc.Texture2DArray.FirstArraySlice = 0;
		rtDesc.Texture2DArray.ArraySize = 1;
	}
	else
	{
		rtDesc.Texture2D.MipSlice = 0;
		rtDesc.Texture2D.PlaneSlice = 0;
	}

	if (layerMipRenderTargetViews12)
	{
		for (int i = 0; i < totalNum; i++)
		{
			rtDesc.Texture2DArray.FirstArraySlice	= i;
			rtDesc.Texture2DArray.ArraySize			= 1;
			for (int j = 0; j < m; j++)
			{
				rtDesc.Texture2DArray.MipSlice = j;

				renderPlatform->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtDesc, mTextureRtHeap.CpuHandle());
				layerMipRenderTargetViews12[i][j] = mTextureRtHeap.CpuHandle();
				mTextureRtHeap.Offset();
			}
		}
	}
	
	rtDesc.Texture2DArray.FirstArraySlice = 0;
	rtDesc.Texture2DArray.ArraySize = totalNum;
	rtDesc.Texture2DArray.MipSlice = 0;
	renderPlatform->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtDesc, mTextureRtHeap.CpuHandle());
	mainRenderTargetView12 = mTextureRtHeap.CpuHandle();
	mTextureRtHeap.Offset();
}

void Texture::FreeDSVTables()
{
	delete[] layerDepthStencilViews12;
	layerDepthStencilViews12 = nullptr;
}
void Texture::InitDSVTables(int l, int m)
{
	layerDepthStencilViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[l];
}
void Texture::CreateDSVTables(int l, int m, bool msaa)
{
	mTextureDsHeap.Restore((dx12::RenderPlatform*)renderPlatform, (l) + (1), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "TextureDSHeap", false);
	bool array = l > 1;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
	dsDesc.Format = (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat, compressionFormat);
	dsDesc.ViewDimension = array ? (msaa ? D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
		: (msaa ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D);
	if (array)
	{
		dsDesc.Texture2DArray.FirstArraySlice = 0;
		dsDesc.Texture2DArray.ArraySize = 1;
	}
	else
	{
		dsDesc.Texture2D.MipSlice = 0;
	}

	if (layerDepthStencilViews12)
	{
		for (int i = 0; i < l; i++)
		{
			dsDesc.Texture2DArray.FirstArraySlice = i;
			dsDesc.Texture2DArray.ArraySize = 1;
			dsDesc.Texture2DArray.MipSlice = 0;

			renderPlatform->AsD3D12Device()->CreateDepthStencilView(mTextureDefault, &dsDesc, mTextureDsHeap.CpuHandle());
			layerDepthStencilViews12[i] = mTextureDsHeap.CpuHandle();
			mTextureDsHeap.Offset();
		}
	}

	dsDesc.Texture2DArray.FirstArraySlice = 0;
	dsDesc.Texture2DArray.ArraySize = l;
	dsDesc.Texture2DArray.MipSlice = 0;
	renderPlatform->AsD3D12Device()->CreateDepthStencilView(mTextureDefault, &dsDesc, mTextureDsHeap.CpuHandle());
	mainDepthStencilView12 = mTextureDsHeap.CpuHandle();
	mTextureDsHeap.Offset();
}

void Texture::InitStateTable(int l, int m)
{
	// Create state table, at this point, we expect that the state
	// has already being set !!!
	auto curState = mResourceState;//GetCurrentState(deviceContext);
	//mSubResourcesStates.clear();
	mSubResourcesStates.resize(l);
	for (int layer = 0; layer < l; layer++)
	{
		mSubResourcesStates[layer].resize(m);
		for (int mip = 0; mip < m; mip++)
		{
			mSubResourcesStates[layer][mip] = curState;
		}
	}
}

void Texture::InvalidateDeviceObjects()
{
	FreeRTVTables();
	FreeUAVTables();
	FreeSRVTables();
	ClearLoadingData();
	// Set the pointer to an invalid value so we can perform checks
	mainShaderResourceView12.ptr	= -1;
	arrayShaderResourceView12.ptr	= -1;
	mainDepthStencilView12.ptr		= -1;
	mainRenderTargetView12.ptr		= -1;

	arraySize				= 0;
	mips					= 0;

	auto rPlat = (dx12::RenderPlatform*)(renderPlatform);
	// We dont want to release the resources of the external texture
	// BUT: we DO want to decrement the refcount, because we added a ref when it was initialized.
	if (mInitializedFromExternal)
	{
		if (mTextureDefault)
			rPlat->PushToReleaseManager(mTextureDefault, (name + "_Default(External)").c_str());
	}
	else
	{
		// Critical resources like textures that could be in use by the GPU will be destroyed by the 
		// release manager
		if(mTextureDefault)
			rPlat->PushToReleaseManager(mTextureDefault, (name + "_Default").c_str());
		if(mTextureUpload)
			rPlat->PushToReleaseManager(mTextureUpload, (name + "_Upload").c_str());
	}

	mTextureDefault=nullptr;
	mTextureUpload=nullptr;
	// Same for heaps (The release method will handle it)
	mTextureSrvHeap.Release();
	mTextureUavHeap.Release();
	mTextureRtHeap.Release();
	mTextureDsHeap.Release();
}

void Texture::LoadFromFile(crossplatform::RenderPlatform *renderPlatform,const char *pFilePathUtf8, bool gen_mips)
{
	const std::vector<std::string> &pathsUtf8=renderPlatform->GetTexturePathsUtf8();
	InvalidateDeviceObjects();

	HRESULT res = S_FALSE;

	// Set initial properties of the texture
	name					= pFilePathUtf8;
	mips					= 1;
	arraySize				= 1;
	depth					= 1;
	cubemap					= false;
	this->renderPlatform	= renderPlatform;

	std::string filename_utf8 = std::string(pFilePathUtf8);
	// Find the path to the file
	std::string strPath;
	strPath = pFilePathUtf8;
	int idx = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(strPath.c_str(), pathsUtf8);
	if (idx<-1 || idx >= (int)pathsUtf8.size())
	{
		std::string file;
		std::vector<std::string> split_path= platform::core::SplitPath( filename_utf8);
		if(split_path.size()>1)
		{
			strPath=split_path[1];
			idx = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(strPath.c_str(), pathsUtf8);
		}
		errno = 0;
		if (idx < -1 || idx >= (int)pathsUtf8.size())
			return;
	}
	errno=0;
	if (idx >= 0)
		strPath = (pathsUtf8[idx] + "/") + strPath;
	 name= strPath;
	 fileContents.resize(1);
	 auto &f=fileContents[0];
	// Load the data
	f.flags		= DirectX::WIC_FLAGS_NONE;
	platform::core::FileLoader::GetFileLoader()->AcquireFileContents(f.ptr,f.bytes, strPath.c_str(), false);
	wicContents.resize(1);
	auto &wic=wicContents[0];
	if(!wic.metadata)
		wic.metadata=new DirectX::TexMetadata();
	if(!wic.scratchImage)
		wic.scratchImage=new DirectX::ScratchImage;
	if(name.find(".hdr")==name.length()-4)
	{
		res= DirectX::LoadFromHDRMemory(f.ptr, (size_t)f.bytes, wic.metadata, *wic.scratchImage);

	}
    else if (name.find(".dds") != std::string::npos)
    {
        res = DirectX::LoadFromDDSMemory(f.ptr, f.bytes, f.flags, wic.metadata, *wic.scratchImage);
    }
    else
    {
	    res = DirectX::LoadFromWICMemory(f.ptr, f.bytes, f.flags, wic.metadata, *wic.scratchImage);
    }
    SIMUL_ASSERT(res == S_OK)
	
	if(res == S_OK)
	{
	// The texture will be considered "valid" from here.
	// Set the properties of this texture
		width			= (int)wic.metadata->width;
		length			= (int)wic.metadata->height;
		if(gen_mips)
		{
			int m=100;
			m = std::min(m, int(floor(log2(std::max(width, length)))) +1);
			m=std::min(16,std::max(1,m));
			mips=m;
		}
		else
			mips=1;
		pixelFormat		=RenderPlatform::FromDxgiFormat(wic.metadata->format);
	}
	mLoadedFromFile = true;
	
	// ok to free loaded data??
	ClearFileContents();
	textureLoadComplete=false;
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if (!textureUploadComplete)
	{
		FinishUploading(deviceContext);
		textureUploadComplete = true;
		textureLoadComplete = true;
		return;
	}
	if(textureLoadComplete)
		return;
	if(mips<0|| mips>16)
		mips=1;
	if (!wicContents.size())
		return;
	auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.DepthOrArraySize	= (UINT16)wicContents.size();
	textureDesc.MipLevels			= mips;
	textureDesc.SampleDesc.Count	= 1;
	textureDesc.SampleDesc.Quality	= 0;
	textureDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let runtime decide
	textureDesc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;	// to generate mips.
	textureDesc.Alignment			= 0;							// Let runtime decide

	// Clear resources
	renderPlatformDx12->PushToReleaseManager(mTextureDefault, (name+" mTextureDefault").c_str());
	renderPlatformDx12->PushToReleaseManager(mTextureUpload, (name+" mTextureUpload").c_str());
	mTextureDefault = nullptr;
	mTextureUpload = nullptr;
	size_t num_loaded=0;
	// Convert into WIC 
	for(int i=0;i<wicContents.size();i++)
	{
		WicContents &wic=wicContents[i];
		HRESULT res=S_OK;
		wic.image   = wic.scratchImage->GetImage(0, 0, 0);
	 
		if (!wic.image)
		{
			SIMUL_BREAK_INTERNAL("Failed to load this texture.");
			continue;
		}

		// Make the texture description
		textureDesc.Width				= (UINT)wic.metadata->width;
		textureDesc.Height				= (UINT)wic.metadata->height;
		textureDesc.Format				= wic.metadata->format;

		if(!mTextureDefault)
		{
			dxgi_format	= wicContents[0].metadata->format;
		// Create the texture resource (GPU, with an implicit heap)
			res = renderPlatform->AsD3D12Device()->CreateCommittedResource
			(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				SIMUL_PPV_ARGS(&mTextureDefault)
			);
			SIMUL_ASSERT(res == S_OK);
			//D3D12_GPU_VIRTUAL_ADDRESS va=mTextureDefault->GetGPUVirtualAddress();
			std::wstring	n = L"GPU_";
			n+=std::wstring(name.begin(), name.end());
			mTextureDefault->SetName(n.c_str());
			size_t texSize = textureDesc.Width * textureDesc.Height  * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
			SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		}
		num_loaded++; 
	}
	if(num_loaded)
	{
		//pixelFormat		=RenderPlatform::FromDxgiFormat(textureDesc.Format);
		// Create an upload heap
		// only single mip in the upload.
		CreateUploadResource(arraySize);
		// Init states
		InitStateTable(arraySize, mips);
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	
		std::vector<D3D12_SUBRESOURCE_DATA> textureSubDatas;
		textureSubDatas.resize(arraySize);
		SetLayout(deviceContext,D3D12_RESOURCE_STATE_COPY_DEST);
		// must flush before copying:
		((dx12::RenderPlatform*)renderPlatform)->FlushBarriers(deviceContext);
		for(int i=0;i<wicContents.size();i++)
		{
			WicContents &wic						=wicContents[i];
			// Perform the texture copy
			D3D12_SUBRESOURCE_DATA &textureSubData	=textureSubDatas[i];
			textureSubData.pData					=wic.image->pixels; 
			textureSubData.RowPitch					=wic.image->rowPitch; 
			textureSubData.SlicePitch				=wic.image->rowPitch * textureDesc.Height;
		
			UpdateSubresources(deviceContext.asD3D12Context(), mTextureDefault, mTextureUpload,pLayouts[i].Offset, i*mips, 1, &textureSubDatas[i]);
		}

		// Create a descriptor heap for this texture
		// This heap won't be shader visible as we will be copying the descriptor from here to the FrameHeap (which is shader visible)
		mTextureSrvHeap.Restore(renderPlatformDx12, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,"TextureCpuHeap",false);

		// Create the descriptor (view) of this texture
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= textureDesc.Format;
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels				= 1;
		renderPlatformDx12->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.GetCpuHandleFromStartAfOffset(0));

		// Create the handle
		mainShaderResourceView12 = mTextureSrvHeap.GetCpuHandleFromStartAfOffset(0);
	
		int totalNum	= cubemap ? 6 * arraySize : arraySize;
		InitSRVTables(totalNum, mips);
		CreateSRVTables(arraySize, mips, cubemap);
		InitRTVTables(totalNum, mips);
		CreateRTVTables(totalNum, mips);
	}
	textureLoadComplete=true;
	textureUploadComplete = true;
	shouldGenerateMips=false;
	if (mips > 1)
	{
		if(deviceContext.deviceContextType!=crossplatform::DeviceContextType::GRAPHICS)
		{
			SIMUL_BREAK_ONCE("Unable to generate mips as context is compute.");
		}
		else
		{
			shouldGenerateMips=true;
		}
	}
	
	SetLayout(deviceContext,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ClearLoadingData();
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files, bool gen_mips)
{
	const std::vector<std::string> &pathsUtf8=r->GetTexturePathsUtf8();
	InvalidateDeviceObjects();

	HRESULT res = S_FALSE;

	// Set initial properties of the texture
	name			= "Array:" + texture_files[0] + ",...";
	arraySize		= (int)texture_files.size();
	depth			= (int)texture_files.size();
	cubemap			= false;
	renderPlatform	= r;
	// Find all the paths
	std::vector<std::string> strPaths;
	strPaths.resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		int idx = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(texture_files[i].c_str(), pathsUtf8);
		if (idx<-1 || idx >= (int)pathsUtf8.size())
			return;
		strPaths[i] = texture_files[i];
		if (idx >= 0)
			strPaths[i] = (pathsUtf8[idx] + "/") + strPaths[i];
	}
	
	ClearLoadingData();
	// Load the data
	fileContents.resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		auto curContents = &fileContents[i];
		platform::core::FileLoader::GetFileLoader()->AcquireFileContents(curContents->ptr, curContents->bytes, strPaths[i].c_str(), false);
	}
	// Convert into WIC 
	wicContents.resize(arraySize);
	for (int i = 0; i < arraySize;i++)
	{
		auto &curContents	= fileContents[i];
		auto &wic			= wicContents[i];
		wic.metadata		=new DirectX::TexMetadata;
		wic.scratchImage	=new DirectX::ScratchImage;
		wic.image		=new DirectX::Image;
        if (texture_files[0].find(".dds") != std::string::npos)
        {
            res = DirectX::LoadFromDDSMemory(curContents.ptr, curContents.bytes, curContents.flags, wic.metadata, *wic.scratchImage);
        }
        else
        {
            res = DirectX::LoadFromWICMemory(curContents.ptr, curContents.bytes, curContents.flags, wic.metadata, *wic.scratchImage);
        }
		width = (int)wic.metadata->width;
		length = (int)wic.metadata->height;
		pixelFormat		=RenderPlatform::FromDxgiFormat(wic.metadata->format);
		wic.image = wic.scratchImage->GetImage(0, 0, 0);
	}

	if (gen_mips)
	{
		int m = 100;
		m = std::min(m, int(floor(log2(std::max(width, length)))) + 1);
		m = std::min(16, std::max(1, m));
		mips = m;
	}
	else
		mips = 1;
	// The texture will be considered "valid" from here.
	// Set the properties of this texture
	mLoadedFromFile = true;
	textureLoadComplete=false;
	textureUploadComplete = true;
}

bool Texture::IsValid() const
{
	if (mTextureDefault != NULL)
		return true;
	if(mLoadedFromFile&&!textureLoadComplete)
		return true;
	return false;
}

ID3D12Resource* Texture::AsD3D12Resource()
{
	return mTextureDefault;
}

void Texture::FinishUploading(crossplatform::DeviceContext& deviceContext)
{
	int totalNum = cubemap ? 6 * arraySize : arraySize;
	SetLayout(deviceContext, D3D12_RESOURCE_STATE_COPY_DEST,-1,-1,true);

	auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
	uint32_t numImages = (uint32_t)upload_data->size();
	CreateUploadResource(totalNum);
	std::vector<D3D12_SUBRESOURCE_DATA> textureSubDatas(numImages);
	int mip_width = width;
	int mip_length = length;
	size_t bytes_per_pixel = dx12::RenderPlatform::ByteSizeOfFormatElement(dxgi_format);
	size_t i=0;
			int m=0;
	for (size_t i=0;i<numImages;i++)
	{
		if(m==mips)
		{
			mip_width = width;
			mip_length = length;
			m=0;
		}
	//for (size_t m=0;m<numImages;m++)
	//{
		//for(size_t a=0;i<totalNum;a++)
	//	{
			//i=m*totalNum+a;
			D3D12_SUBRESOURCE_DATA& textureSubData = textureSubDatas[i];
			textureSubData.pData = (*upload_data)[i].data();
			textureSubData.RowPitch = mip_width * bytes_per_pixel;
			textureSubData.SlicePitch = textureSubData.RowPitch * mip_length;

			static int uu = 4;
			switch (compressionFormat)
			{
			case crossplatform::CompressionFormat::BC1:
			case crossplatform::CompressionFormat::BC3:
			case crossplatform::CompressionFormat::BC5:
				textureSubData.RowPitch = bytes_per_pixel * (mip_width / uu);
				textureSubData.SlicePitch = textureSubData.RowPitch * mip_length / 4;
				break;
			default:
				break;
			};
			mip_width = (mip_width + 1) / 2;
			mip_length = (mip_length + 1) / 2;
			m++;
		//}
	}
	//renderPlatformDx12->ResourceTransitionSimple(deviceContext, mTextureDefault, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST, true);
	
	UpdateSubresources(deviceContext.asD3D12Context(), mTextureDefault, mTextureUpload, 0, 0, numImages, textureSubDatas.data());
	// no need to flush?
	SetLayout(deviceContext, D3D12_RESOURCE_STATE_GENERIC_READ,-1,-1,true);
//	renderPlatformDx12->ResourceTransitionSimple(deviceContext, mTextureDefault, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	upload_data = nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12ShaderResourceView(crossplatform::DeviceContext &deviceContext,bool setState /*= true*/, crossplatform::ShaderResourceType t, int index, int mip,bool pixel_shader)
{
    if (mip >= mips)
    {
        mip = 0;
    }
	if(!textureLoadComplete)
	{
		FinishLoading(deviceContext);
		textureLoadComplete=true;
	}
	if (!textureUploadComplete)
	{
		std::cout << "Finish Uploading texture " << name.c_str() << std::endl;
		FinishUploading(deviceContext);
		textureUploadComplete = true;
	}
	// Ensure a valid state for the resource
	if (setState)
	{
		auto curState = GetCurrentState(deviceContext,mip,index);
		auto newState=pixel_shader?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if	((curState & (newState)) != (newState) )
		{
			SetLayout(deviceContext,newState,mip,index);
		}
	}

	bool no_array = !cubemap && (arraySize <= 1);

	// Return array SRV / return main SRV
	if (mips <= 1 && no_array || (index < 0 && mip < 0))
	{
        if (IsCubemap() && t == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
        {
			return &arrayShaderResourceView12;
        }

		if (yuvLayerIndex < 0)
		{
			return &mainShaderResourceView12;
		}

		if (yuvLayerIndex == 0)
		{
			yuvLayerIndex = 1;
			return &mainShaderResourceView12;
		}

		yuvLayerIndex = 0;
		return &uvLayerShaderResourceView12;
	}

	// Return main SRV / return element of array
	if (layerShaderResourceViews12 && (mip < 0 || mips <= 1))
	{
        if (index < 0 || no_array)
        {
			return &mainShaderResourceView12;
        }
		return &layerShaderResourceViews12[index];
	}

	// Return main SRV to a MIP
    if (mainMipShaderResourceViews12 && (no_array || index < 0))
    {
		return &mainMipShaderResourceViews12[mip];
    }

	// Return main SRV to a MIP of a layer
    if (layerMipShaderResourceViews12)
    {
		return &layerMipShaderResourceViews12[index][mip];
    }

	return nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext,int index, int mip)
{
    if (mip >= mips)
    {
        mip = 0;
    }
	// Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext,mip, index);
	if ((curState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		SetLayout(deviceContext,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,mip,index);
	}

    if (mip < 0)
	{
		mip = 0;
	}
    if (index < 0)
	{
        if (mipUnorderedAccessViews12)
        {
			return &mipUnorderedAccessViews12[mip];		// UAV for the whole texture at various mips
        }
        else
        {
            index = 0;
        }
	}
    if (!layerMipUnorderedAccessViews12)
    {
		return nullptr;
    }
	return &layerMipUnorderedAccessViews12[index][mip];
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12DepthStencilView(crossplatform::DeviceContext &deviceContext, int index, int mip)
{
	// We need a valid render platform
	if (!renderPlatform)
	{
		SIMUL_CERR_ONCE << "This texture has not been created yet but we are trying to get a view: " << name << std::endl;
		return nullptr;
	}
	// Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext);
	if ((curState & D3D12_RESOURCE_STATE_DEPTH_WRITE) != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
		SetLayout(deviceContext,D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	if (mip == mips)
	{
		mip = 0;
	}
	int realArray = GetArraySize();
	bool no_array = !cubemap && (arraySize <= 1);

	// Base view:
	if ((mips <= 1 && no_array) || (index < 0 && mip < 0))
	{
		return &layerDepthStencilViews12[0];
	}
	if (arraySize > 1 && index < 0 && mip == 0)
	{
		return &mainDepthStencilView12;
	}
	if (mip > 0)
	{
		SIMUL_BREAK("Not implemented.");
	}
	return &layerDepthStencilViews12[index];
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12RenderTargetView(crossplatform::DeviceContext &deviceContext,int index, int mip)
{
	// Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext,mip, index);
	if ((curState & D3D12_RESOURCE_STATE_RENDER_TARGET) != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		SetLayout(deviceContext,D3D12_RESOURCE_STATE_RENDER_TARGET,mip,index);
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	}
	
	if (mip == mips)
	{
		mip = 0;
	}
	int realArray = GetArraySize();
	bool no_array = !cubemap && (arraySize <= 1);

	// Base view:
	if ((mips <= 1 && no_array) || (index < 0 && mip < 0))
	{
		return &layerMipRenderTargetViews12[0][0];
	}
	if (arraySize > 1 && index < 0 && mip == 0)
	{
		return &mainRenderTargetView12;
	}
	if (index < 0)
		index = 0;
	if (mip < 0)
		mip = 0;
	return &layerMipRenderTargetViews12[index][mip];
}

void Texture::copyToMemory(crossplatform::DeviceContext &,void *,int ,int )
{
	SIMUL_BREAK_ONCE("This is not implemented yet.");
}

void Texture::setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)
{
	HRESULT res = S_FALSE;
	
	if (mLoadedFromFile)
	{
		SIMUL_BREAK_ONCE("This is not yet supported for textures loaded from file.");
		return;
	}
	else
	{
		auto renderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
		
		if (!mTextureDefault)
		{
			SIMUL_BREAK_INTERNAL("The texture is invalid");
			return;
		}
		renderPlat->PushToReleaseManager(mTextureUpload, "TextureSetTexelsUpload");

		// We neen to know the size of the texture (DEFAULT)
		UINT64 texSize = 0;
		renderPlat->AsD3D12Device()->GetCopyableFootprints
		(
			&mTextureDefault->GetDesc(),
			0, 1, 0,
			nullptr, nullptr, nullptr,
			&texSize
		);

		// Create the upload buffer
		res = renderPlat->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(texSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
            SIMUL_PPV_ARGS(&mTextureUpload)
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mTextureUpload, texSize)
		mTextureUpload->SetName(L"TextureSetTexelsUpload");

		// Parameters
		int texelSize	= RenderPlatform::ByteSizeOfFormatElement(dxgi_format);
		int srcPitch	= texelSize * width;
		int srcSlice	= srcPitch * length;

		// Checks
		if (texel_index != 0)
			SIMUL_BREAK_INTERNAL("Nacho has to implement this")
		if (srcSlice != (texelSize * (width * length)))
			SIMUL_BREAK_INTERNAL("Nacho has to implement this")

		// Transition main texture to copy dest
		renderPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, GetCurrentState(deviceContext), D3D12_RESOURCE_STATE_COPY_DEST,true);

		// Copy the data
		D3D12_SUBRESOURCE_DATA srcData;
		srcData.pData			= src;
		srcData.RowPitch		= srcPitch;
		srcData.SlicePitch		= srcSlice;
		UpdateSubresources(deviceContext.asD3D12Context(), mTextureDefault, mTextureUpload, 0, 0, 1, &srcData);

		// Transition main texture to pixel shader resource
		renderPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, D3D12_RESOURCE_STATE_COPY_DEST, GetCurrentState(deviceContext),true);
	}
}

bool Texture::IsComputable() const
{
	return (mipUnorderedAccessViews12 != nullptr || layerMipUnorderedAccessViews12 != nullptr);
}

bool Texture::HasRenderTargets() const
{
	return (layerMipRenderTargetViews12 != nullptr) || (mainRenderTargetView12.ptr != -1);
}

bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt, bool setDepthStencil,bool need_srv, int numOfSamples)
{
	return InitFromExternalD3D12Texture2D(renderPlatform,(ID3D12Resource*)t,(D3D12_CPU_DESCRIPTOR_HANDLE*)srv,make_rt,setDepthStencil,need_srv);
}

void Texture::SetName(const char *n)
{
	crossplatform::Texture::SetName(n);
	if(!mTextureDefault)
		return;
	std::wstring ws=platform::core::Utf8ToWString(n);
	mTextureDefault->SetName(ws.c_str());
}

void Texture::StoreExternalState(crossplatform::ResourceState resourceState)
{
	mExternalLayout=(D3D12_RESOURCE_STATES)resourceState;
	if(resourceState==crossplatform::ResourceState::UNKNOWN)
		return;
	AssumeLayout(mExternalLayout);
}

bool Texture::InitFromExternalD3D12Texture2D(crossplatform::RenderPlatform* r, ID3D12Resource * t, D3D12_CPU_DESCRIPTOR_HANDLE * srv, bool make_rt, bool setDepthStencil,bool need_srv)
{
	//Check that the texture pointer is valid.
	if (t)
	{
		ID3D12Resource* _t;
		if (t->QueryInterface(__uuidof(ID3D12Resource), (void**)&_t) != S_OK)
		{
			SIMUL_CERR << "Can't initialise from external texture: 0x" << std::hex << t << std::dec << std::endl;
			SIMUL_BREAK_ONCE("Not a valid D3D Texture");
			return false;
		}
		SAFE_RELEASE(_t);
	}
	renderPlatform = r;
	mExternalLayout=D3D12_RESOURCE_STATE_COMMON;
	// If it's the same as before, return.
	if ((mTextureDefault == t && srv && srv->ptr == mainShaderResourceView12.ptr) && mainShaderResourceView12.ptr != -1 
		&& (make_rt == (layerMipRenderTargetViews12 != NULL) || (mainRenderTargetView12.ptr != -1)))
	{
		return true;
	}
	// If it's the same texture, and we created our own srv, that's fine, return.
	if (mTextureDefault != NULL && mTextureDefault == t && mainShaderResourceView12.ptr != -1 && srv == NULL)
	{
		return true;
	}
	if (mTextureDefault)
	{
		auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
		// Pass "owned=false" so we don't try to untrack memory we never tracked.
		renderPlatformDx12->PushToReleaseManager(mTextureDefault, (name+" changing mTextureDefault").c_str(),!mInitializedFromExternal);
		
	}
	if (!t)
	{
		return false;
	}
	t->AddRef();
	FreeSRVTables();
	mTextureDefault				= t;
	mainShaderResourceView12	= srv? *srv : D3D12_CPU_DESCRIPTOR_HANDLE(); // What if the CPU handle changes? we should check this from outside
	mInitializedFromExternal	= true;

	// Textures initialized from external should be passed by as a SRV so we expect
	// that the resource was previously transitioned to GENERIC_READ

	if (mTextureDefault)
	{
		D3D12_RESOURCE_DESC textureDesc = mTextureDefault->GetDesc();
	
		// Assume cubemap
		if (textureDesc.DepthOrArraySize == 6 && (textureDesc.Width == textureDesc.Height))
		{
			cubemap							= true;
			textureDesc.DepthOrArraySize	= 1;
		}
		else
		{
			cubemap							= false;
		}
		dxgi_format = RenderPlatform::DsvToTypelessFormat(textureDesc.Format);
		
		pixelFormat = RenderPlatform::FromDxgiFormat(dxgi_format);
		compressionFormat=RenderPlatform::DxgiFormatToCompressionFormat(dxgi_format);
		InitFormats(pixelFormat);
		width		= (int)textureDesc.Width;
		length		= (int)textureDesc.Height;
        mNumSamples = textureDesc.SampleDesc.Count;
		InitStateTable(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
		if (!srv&&(textureDesc.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)==0)
		{
			InitSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
			CreateSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels, cubemap, false, textureDesc.SampleDesc.Count > 1);
			arraySize	= textureDesc.DepthOrArraySize;
			mips		= textureDesc.MipLevels;
		}
		//InitSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels); //Depth Texture was not having its InitStateTable updating, thus mSubResourcesStates.size() was 0. -AJR
		depth       = textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? textureDesc.DepthOrArraySize : 1;

		// Create render target views for this external texture:
		if (make_rt && !setDepthStencil)
		{
			if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
			{
				bool msaa = (textureDesc.SampleDesc.Count) > 1;

				FreeRTVTables();
				InitRTVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
				CreateRTVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels, msaa);
			}
			else
			{
				SIMUL_CERR << "This external texture can not be created as a render target. The flags do not support it.\n";
				return false;
			}
		}

        // Create depth stencil views from this texture:
        if (setDepthStencil)
        {
            if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            {
				bool msaa = (textureDesc.SampleDesc.Count) > 1;

				FreeDSVTables();
				InitDSVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
				CreateDSVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels, msaa);

                depthStencil = true;
            }
            else
            {
                SIMUL_CERR << "This external texture can not be created as a depth stencil. The flags do not support it.\n";
            }
        }
	}
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	dim			= 2;
	return true;
}

bool Texture::InitFromExternalTexture3D(crossplatform::RenderPlatform *r,void *ta,void *srv,bool make_uav)
{
	return false;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int d,crossplatform::PixelFormat pf,bool computable,int m,bool rendertargets)
{
	HRESULT res = S_FALSE;

	renderPlatform  = r;
	bool ok			= true;

	if (mTextureDefault)
	{
		D3D12_RESOURCE_DESC desc = mTextureDefault->GetDesc();
		if (dim!=3||desc.Width != w || desc.Height != l || desc.DepthOrArraySize != d || desc.MipLevels != m || pixelFormat != pf)
		{
			int mindim=std::min(std::min(w,l),d);
			while(m>1&&(1<<(m-1))>mindim)
			{
				m--;
			}
			if (desc.Width != w || desc.Height != l || desc.DepthOrArraySize != d || desc.MipLevels != m || pixelFormat != pf)
			{
				ok = false;
			}
		}
		if (computable != ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
			ok = false;
		if (rendertargets != ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
			ok = false;
	}
	else
	{
		ok = false;
	}
	bool changed = !ok;
	if (!ok)
	{
		InvalidateDeviceObjects();
		dim				= 3;
		InitFormats(pf);
		int mindim=std::min(std::min(w,l),d);
		while(m>1&&(1<<(m-1))>mindim)
		{
			m--;
		}
		dxgi_format = genericDxgiFormat;
		width		= w;
		length		= l;
		depth		= d;

		SIMUL_ASSERT(w > 0 && l > 0 && w <= 65536 && l <= 65536);

		D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
		textureFlags = (D3D12_RESOURCE_FLAGS)
		(
			textureFlags | 
			(computable		? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	: 0) |
			(rendertargets	? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		: 0)
		);

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex3D
		(
			dxgi_format,
			width,
			length,
			depth,
			m,
			textureFlags
		);

		// Make the clear values
		D3D12_CLEAR_VALUE clearValues = {};
		if (rendertargets)
		{
			clearValues.Format		= genericDxgiFormat;
			clearValues.Color[0]	= 1.0f;
			clearValues.Color[1]	= 0.0f;
			clearValues.Color[2]	= 0.0f;
			clearValues.Color[3]	= 1.0f;
		}

		// Find the initial texture state
		D3D12_RESOURCE_STATES initialState=D3D12_RESOURCE_STATE_GENERIC_READ;

		// Clean resources
		SAFE_RELEASE_LATER(mTextureDefault); 


		res = r->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			initialState,
			rendertargets? &clearValues : nullptr,
            SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l * d * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());
		AssumeLayout(initialState);

		// Create the main SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= genericDxgiFormat;
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels				= m;
		srvDesc.Texture3D.MostDetailedMip		= 0;
		InitStateTable(1,m);

		FreeSRVTables();
		InitSRVTables(1, m);

		if (name.length() == 0)
			SIMUL_BREAK_INTERNAL("Unnamed texture")

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)r, 1 + m, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (name+" Texture3DSrvHeap").c_str(), false);
		if(mTextureSrvHeap.GetCount()==0)
		{
			mTextureSrvHeap.Restore((dx12::RenderPlatform*)r, 1 + m, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (name+" Texture3DSrvHeap").c_str(), false);
		}
		r->AsD3D12Device()->CreateShaderResourceView(mTextureDefault,&srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if(mainMipShaderResourceViews12)
		for (int j = 0; j<m; j++)
		{
			srvDesc.Texture3D.MipLevels = 1;
			srvDesc.Texture3D.MostDetailedMip = j;
			
			r->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
			mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
			mTextureSrvHeap.Offset();
		}

		if (computable && (!layerMipUnorderedAccessViews12 || !ok))
		{
			FreeUAVTables();
			InitUAVTables(1, m);
			changed = true;
			mTextureUavHeap.Restore((dx12::RenderPlatform*)r, m * 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,(name+ " Texture3DUavHeap").c_str(), false);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
			uavDesc.Format								= genericDxgiFormat;
			uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice					= 0;
			uavDesc.Texture3D.WSize						= d;
			uavDesc.Texture3D.FirstWSlice				= 0;

			if (mipUnorderedAccessViews12)
			for (int i = 0; i < m; i++)
			{
				uavDesc.Texture3D.MipSlice = i;
				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault,nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
				uavDesc.Texture3D.WSize /= 2;
			}

			uavDesc.Texture3D.WSize = d;
			if (layerMipUnorderedAccessViews12)
			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture3D.MipSlice = i;
				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				layerMipUnorderedAccessViews12[0][i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
				uavDesc.Texture3D.WSize /= 2;
			}
		}
		if (d <= 8 && rendertargets && (!layerMipRenderTargetViews12 || !layerMipRenderTargetViews12[0] || mainRenderTargetView12.ptr == -1 || !ok))
		{
			SIMUL_BREAK_INTERNAL("Render targets for 3D textures are not currently supported.");
		}
	}

	mips		= m;
	arraySize	= 1;
	return changed;
}

bool Texture::ensureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l, int m,
											crossplatform::PixelFormat f
											, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
											, bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil, bool shared, crossplatform::CompressionFormat compressionFormat)
{
	return EnsureTexture2DSizeAndFormat(r,w,l,m,f,data,computable,rendertarget,depthstencil,num_samples,aa_quality,wrap,clear,clearDepth,clearStencil,shared,compressionFormat);
}
void Texture::InitFormats(crossplatform::PixelFormat f)
{
	pixelFormat		= f;
	dxgi_format		= (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	genericDxgiFormat = dxgi_format;
	srvFormat		= dxgi_format;
	// needed for video decoder.
	yuvFormat = false;
	if(genericDxgiFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		srvFormat	=genericDxgiFormat;
		genericDxgiFormat	=DXGI_FORMAT_R8G8B8A8_TYPELESS;
		uavFormat	=DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D32_FLOAT)
	{
		genericDxgiFormat = DXGI_FORMAT_R32_TYPELESS;
		srvFormat		= DXGI_FORMAT_R32_FLOAT;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
	{
		genericDxgiFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		srvFormat		= DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D16_UNORM)
	{
		genericDxgiFormat = DXGI_FORMAT_R16_TYPELESS;
		srvFormat		= DXGI_FORMAT_R16_UNORM;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		genericDxgiFormat = DXGI_FORMAT_R24G8_TYPELESS;
		srvFormat		= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	if (genericDxgiFormat == DXGI_FORMAT_NV12)
	{
		// For Y layer. UINT is easier for conversion to RGB in shader than UNORM.
		srvFormat = DXGI_FORMAT_R8_UINT;
		yuvFormat = true;
	}
}

bool Texture::EnsureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l, int m
											,crossplatform::PixelFormat f
											, std::shared_ptr<std::vector<std::vector<uint8_t>>> data
											,bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil, bool shared
											,crossplatform::CompressionFormat compr)
{
	// Define pixel formats of this texture
	renderPlatform	= r;
	dim			= 2;
	HRESULT res = S_FALSE;
	
	bool ok = true;
	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
        if (desc.Width != w || desc.Height != l || desc.MipLevels != m || f != pixelFormat||mips!=m)
        {
			ok = false;
        }
        if (computable != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        {
			ok = false;
        }
        if (rendertarget != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
        {
			ok = false;
        }
        if (depthstencil != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        {
			ok = false;
        }
        if (desc.SampleDesc.Count != num_samples)
        {
            ok = false;
        }
		if (compressionFormat != compr)
			ok = false;
	}
	else
	{
		ok = false;
	}

	if (!ok)
	{
		if (w*l <= 0)
			return false;
		InvalidateDeviceObjects();
		pixelFormat		= f;
		compressionFormat = compr;
		InitFormats(f);
		width = w;
		length = l;
		mips		= m;
		arraySize	= 1;

		// Check msaa support
		if (num_samples > 1)
		{
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQuery = {};
			msaaQuery.SampleCount		= num_samples;
			msaaQuery.NumQualityLevels	= aa_quality;
			msaaQuery.Format			= srvFormat;
			res                         = renderPlatform->AsD3D12Device()->CheckFeatureSupport
            (
                D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQuery, sizeof(msaaQuery)
            );
			if (FAILED(res))
			{
				SIMUL_CERR << "The provided quality settings are not supported by this device. \n";
				num_samples = 1;
				aa_quality	= 0;
			}
            mNumSamples = num_samples;
		}

		D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
		textureFlags = (D3D12_RESOURCE_FLAGS)
		(
			textureFlags | 
			(computable		? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	: 0) |
			(rendertarget	? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		: 0) |
			(depthstencil	? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL		: 0)
		);

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			genericDxgiFormat,
			width,
			length,
			1,
			m,
			num_samples,
			aa_quality,
			textureFlags
		);

		// Make the clear values
		D3D12_CLEAR_VALUE clearValues = {};
		if (rendertarget || depthstencil)
		{
			clearValues.Format = genericDxgiFormat;
			if (depthstencil)
			{
				clearValues.Format					= dxgi_format;	// Clear format can't be typeless
				clearValues.DepthStencil.Depth		= clearDepth;
				clearValues.DepthStencil.Stencil	= clearStencil;
			}
			else if (!depthstencil && rendertarget)
			{
				clearValues.Color[0] = clear.x;
				clearValues.Color[1] = clear.y;
				clearValues.Color[2] = clear.z;
				clearValues.Color[3] = clear.w;
			}
		}

		// Clean resources
		auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
		renderPlatformDx12->PushToReleaseManager(mTextureDefault,  (name+" mTextureDefault").c_str());
		mTextureDefault = nullptr;


		D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_NONE;
		if (shared)
		{
			heapFlags = D3D12_HEAP_FLAGS::D3D12_HEAP_FLAG_SHARED;
		}

		// Create the texture resource
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			heapFlags,
			&textureDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			(rendertarget || depthstencil) ? &clearValues : nullptr,
            SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_COUT << "Creating texture "<<name.c_str()<<". "<<"\n";
		SIMUL_ASSERT(res == S_OK);
		auto desc = mTextureDefault->GetDesc();
		size_t texSize;
		size_t bytes_per_pixel = 0;
		size_t sizeInPixels = w * l;
		if (yuvFormat)
		{
			// Y * UV	
			texSize = sizeInPixels + (sizeInPixels / 2);
		}
		else
		{
			bytes_per_pixel = dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format);
			texSize = sizeInPixels * bytes_per_pixel;
		}
		
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
#if SIMUL_INTERNAL_CHECKS
		if (name.length() == 0)
		{
			SIMUL_CERR << "Unnamed texture!" << std::endl;
		}
#endif
		mTextureDefault->SetName(n.c_str());
		
		if (data)
			textureUploadComplete = false;
		else
			textureUploadComplete = true;
		textureLoadComplete = true;
		upload_data = data;

		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
		InitStateTable(1, m);
		// Create the main SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= srvFormat;
		srvDesc.ViewDimension					= num_samples > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels				= 1;
		if(name.length()==0)
		{
			SIMUL_BREAK_INTERNAL("Unnamed texture");
		}

		uint32_t srvCount;
		if (yuvFormat)
		{
			srvCount = 2;
			srvDesc.Texture2D.PlaneSlice = 0;
		}
		else
		{
			srvCount = 1;
		}
	/*	mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, srvCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, platform::core::QuickFormat("Texture2DSrvHeap %s",name.c_str()), false);
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();*/
		
		FreeSRVTables();
		InitSRVTables(1, m);
		CreateSRVTables(1, m, false);
		// Create the UV layer SRV
		if (yuvFormat)
		{
			srvDesc.Format = DXGI_FORMAT_R8G8_UINT;
			srvDesc.Texture2D.PlaneSlice = 1;
			renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
			uvLayerShaderResourceView12 = mTextureSrvHeap.CpuHandle();
			mTextureSrvHeap.Offset();
			yuvLayerIndex = 0;
		}

		if (computable && (!layerMipUnorderedAccessViews12 || !ok))
		{
			if (m < 1)
				m = 1;
			FreeUAVTables();
			InitUAVTables(1, m);
			mTextureUavHeap.Restore((dx12::RenderPlatform*)renderPlatform, m * 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, platform::core::QuickFormat("Texture2DUavHeap %s",name.c_str()), false);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
			uavDesc.Format								= genericDxgiFormat;
			uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_TEXTURE2D;

			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture2D.MipSlice = i;
				renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault,nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}

			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture2D.MipSlice = i;
				renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				layerMipUnorderedAccessViews12[0][i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}

		}
		if (rendertarget && (!layerMipRenderTargetViews12 || mainRenderTargetView12.ptr == -1 || !ok))
		{
			FreeRTVTables();
			InitRTVTables(1, m);
			CreateRTVTables(1, m, num_samples > 1);
		}
		if (depthstencil && (!ok))
		{
			FreeDSVTables();
			InitDSVTables(1, m);
			CreateDSVTables(1, m, num_samples > 1);
		}
	}
	
	return !ok;
}

bool Texture::ensureVideoTexture(crossplatform::RenderPlatform* r, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType)
{
#if SIMUL_D3D12_VIDEO_SUPPORTED
	// Define pixel formats of this texture
	renderPlatform = r;
	pixelFormat = f;
	compressionFormat = crossplatform::CompressionFormat::UNCOMPRESSED;
	InitFormats(f);
	dxgi_format = (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	genericDxgiFormat = dxgi_format;
	if (genericDxgiFormat == DXGI_FORMAT_D32_FLOAT)
	{
		genericDxgiFormat = DXGI_FORMAT_R32_TYPELESS;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D16_UNORM)
	{
		genericDxgiFormat = DXGI_FORMAT_R16_TYPELESS;
	}
	if (genericDxgiFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		genericDxgiFormat = DXGI_FORMAT_R24G8_TYPELESS;
	}
	dim = 2;
	HRESULT res = S_FALSE;

	int mipLevels = 1;

	bool ok = true;
	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
		if (desc.Width != w || desc.Height != l || desc.MipLevels != mipLevels || desc.Format != genericDxgiFormat)
		{
			ok = false;
		}
		else if (!(desc.Flags & (D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
		{
			ok = false;
		}
	}
	else
	{
		ok = false;
	}

	if (!ok)
	{
		if (w * l <= 0)
			return false;
		width = w;
		length = l;

		InvalidateDeviceObjects();
		

		D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
		if (texType == crossplatform::VideoTextureType::ENCODE)
		{
			initialState = D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;
		}
		else if (texType == crossplatform::VideoTextureType::DECODE)
		{
			initialState = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
			textureFlags |= D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY;
		}
		else if (texType == crossplatform::VideoTextureType::PROCESS)
		{
			initialState = D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE;
		}

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			genericDxgiFormat,
			width,
			length,
			1,
			mipLevels,
			1,
			1,
			textureFlags
		);

		// Clean resources
		auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
		renderPlatformDx12->PushToReleaseManager(mTextureDefault, (name+" mTextureDefault").c_str());
		mTextureDefault = nullptr;
			
		// Create the texture resource
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			initialState,
			nullptr,
			SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
			std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());

		AssumeLayout(initialState);
		InitStateTable(1, mipLevels);	
	}

	mips = 1;
	arraySize = 1;
	return true;
#else
	return false;
#endif
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform* r, int w, int l, int num, int m, crossplatform::PixelFormat f
	, std::shared_ptr<std::vector<std::vector<uint8_t>>> data, bool computable, bool rendertarget, bool depthstencil, bool cubemap, crossplatform::CompressionFormat compr)
{
	bool ok = true;
	int totalNum = cubemap ? 6 * num : num;

	if (mTextureDefault)
	{
		auto tDesc = mTextureDefault->GetDesc();
		if (tDesc.DepthOrArraySize != totalNum || tDesc.MipLevels != m || tDesc.Width != w || tDesc.Height != l || pixelFormat != f)
		{
			ok = false;
		}
		if (computable != ((tDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
		{
			ok = false;
		}
		if (rendertarget != ((tDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
		{
			ok = false;
		}
		if (depthstencil != ((tDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			ok = false;
		}
	}
	else
	{
		ok = false;
	}
	if (ok)
	{
		return false;
	}
	HRESULT res = S_OK;
	renderPlatform = r;
	pixelFormat = f;
	compressionFormat = compr;
	InitFormats(f);
	dxgi_format = dx12::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	width = w;
	length = l;
	depth = 1;
	dim = 2;
	mips = m;
	arraySize = num;
	this->cubemap = cubemap;
	this->renderTarget = renderTarget;
	this->depthStencil = depthstencil;
	this->computable = computable;

	D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
	textureFlags = (D3D12_RESOURCE_FLAGS)
		(
			textureFlags |
			(computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : 0) |
			(rendertarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : 0) |
			(depthstencil ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL :0)
			);

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		dxgi_format,
		width,
		length,
		totalNum,
		m,
		1,
		0,
		textureFlags
	);

	// Make the clear values
	// Make the clear values
	D3D12_CLEAR_VALUE clearValues = {};
	if (rendertarget || depthstencil)
	{
		clearValues.Format = genericDxgiFormat;
		if (depthstencil)
		{
			clearValues.Format = dxgi_format;	// Clear format can't be typeless
			clearValues.DepthStencil.Depth = 0.0f;
			clearValues.DepthStencil.Stencil = 0;
		}
		else if (!depthstencil && rendertarget)
		{
			clearValues.Color[0] = 0.0f;
			clearValues.Color[1] = 0.0f;
			clearValues.Color[2] = 0.0f;
			clearValues.Color[3] = 0.0f;
		}
	}

	// Clear resources
	SAFE_RELEASE_LATER(mTextureDefault);
	// Find the initial texture state
	if (rendertarget)
	{
		AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	else if (depthstencil)
	{
		AssumeLayout(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	else if (computable)
	{
		AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	else
	{
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	}
	res = r->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		mResourceState,
		(rendertarget || depthstencil) ? &clearValues : nullptr,
        SIMUL_PPV_ARGS(&mTextureDefault)
	);
	SIMUL_ASSERT(res == S_OK);
	size_t texSize = w * l * totalNum * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
	SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
	std::wstring n = L"GPU_";
	n += std::wstring(name.begin(), name.end());
	mTextureDefault->SetName(n.c_str());

	FreeSRVTables();
	FreeRTVTables();
	FreeDSVTables();
	FreeUAVTables();
	
	InitStateTable(totalNum, m);
	InitSRVTables(totalNum, m);
	CreateSRVTables(num, m, cubemap);
	
	if (computable)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		uavDesc.Format							= dxgi_format;
		uavDesc.ViewDimension					= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize		= totalNum;
		uavDesc.Texture2DArray.FirstArraySlice	= 0;

		InitUAVTables(totalNum, m);

		// Restore UAV heap
		uint heapSize = (mipUnorderedAccessViews12 ? m : 0) + (layerMipUnorderedAccessViews12 ? totalNum * m : 0);
		mTextureUavHeap.Restore((dx12::RenderPlatform*)r, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TextureUAVHeap", false);

		if (mipUnorderedAccessViews12)
		{
			for (int i = 0; i < m; i++)
			{
				uavDesc.Texture2DArray.MipSlice = i;

				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}
		}
		if (layerMipUnorderedAccessViews12)
		{
			for (int i = 0; i < totalNum; i++)
			{
				for (int j = 0; j < m; j++)
				{
					uavDesc.Texture2DArray.FirstArraySlice	= i;
					uavDesc.Texture2DArray.ArraySize		= 1;
					uavDesc.Texture2DArray.MipSlice			= j;

					r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
					layerMipUnorderedAccessViews12[i][j] = mTextureUavHeap.CpuHandle();
					mTextureUavHeap.Offset();
				}
			}
		}
	}
	if (rendertarget)
	{
		InitRTVTables(totalNum, m);
		CreateRTVTables(totalNum, m);
	}
	if (depthstencil)
	{
		InitDSVTables(totalNum, m);
		CreateDSVTables(totalNum, m);
	}

	auto rPlat		= (dx12::RenderPlatform*)renderPlatform;
	//rPlat->FlushBarriers(deviceContext);


	if (data)
		textureUploadComplete = false;
	else
		textureUploadComplete = true;
	textureLoadComplete = true;
	upload_data = data;
	return true;
}



void Texture::ensureTexture1DSizeAndFormat(ID3D12Device* pd3dDevice,int w,crossplatform::PixelFormat pf,bool computable)
{
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = nullptr;
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
		dsv = AsD3D12DepthStencilView(deviceContext, -1, 0);
	else
		dsv = AsD3D12DepthStencilView(deviceContext);

	const D3D12_CLEAR_FLAGS kFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
	deviceContext.asD3D12Context()->ClearDepthStencilView(*dsv, kFlags, depthClear, stencilClear, 0, nullptr);
}

void Texture::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (mips <= 1)
	{
		return;
	}
	deviceContext.renderPlatform->GenerateMips(deviceContext, this, true);
}


bool Texture::isMapped() const
{
	return false;
}

void Texture::unmap()
{
}

vec4 Texture::GetTexel(crossplatform::DeviceContext &,vec2 ,bool )
{
	SIMUL_BREAK_INTERNAL("");
	return vec4(0.0f,0.0f,0.0f,0.0f);
}

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext,int array_index,int mip)
{
    if (array_index < 0)
    {
		array_index = 0;
    }
    if (mip < 0)
    {
		mip = 0;
    }
    if (mip >= mips)
    {
		return;
    }
	
	SetLayout(deviceContext,D3D12_RESOURCE_STATE_RENDER_TARGET,mip,array_index);
	if (layerMipRenderTargetViews12)
	{
		auto rp		= (dx12::RenderPlatform*)deviceContext.renderPlatform;
		rp->FlushBarriers(deviceContext);
		auto rtView = AsD3D12RenderTargetView(deviceContext,array_index, mip);

        targetsAndViewport.num				= 1;
        targetsAndViewport.m_dt				= nullptr;
        targetsAndViewport.m_rt[0]			= rtView;
        targetsAndViewport.rtFormats[0]     = pixelFormat;
        targetsAndViewport.viewport.w		= (int)(std::max(1, (width >> mip)));
        targetsAndViewport.viewport.h		= (int)(std::max(1, (length >> mip)));
        targetsAndViewport.viewport.x		= 0;
        targetsAndViewport.viewport.y		= 0;

        rp->ActivateRenderTargets(deviceContext, &targetsAndViewport);
        
        // Inform the render platform of the current number of MSAA samples
        mCachedMSAAState = rp->GetMSAAInfo();
        rp->SetCurrentSamples(mNumSamples);
	}
}

void Texture::deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext)
{
	auto rp = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	if(deviceContext.GetFrameBufferStack().top()!=&targetsAndViewport)
	{
		SIMUL_BREAK_ONCE("Trying to deactivate a texture that wasn't bound as rendertarget.");
		return;
	}
    rp->DeactivateRenderTargets(deviceContext);
    // Restore MSAA state
    rp->SetCurrentSamples(mCachedMSAAState.Count,mCachedMSAAState.Quality);
}

int Texture::GetSampleCount()const
{
    return mNumSamples == 1 ? 0 : mNumSamples;
}

D3D12_RESOURCE_STATES Texture::GetCurrentState(crossplatform::DeviceContext &deviceContext,int mip /*= -1*/, int index /*= -1*/)
{
	if(!split_layouts)
		return mResourceState;
	// Return the resource state of a mip or array layer, or the whole resource
	if (mip == -1 || index == -1)
	{
		bool allLayers = index == -1 ? true : false;
		bool allMips = mip == -1 ? true : false;
		int numLayers = allLayers ? (int)mSubResourcesStates.size() : 1;
		int numMips = allMips ? mips : 1;
		int startLayer = allLayers ? 0 : index;
		int startMip = allMips ? 0 : mip;

		// If we request the state of the ranges of subresource, we have to make sure
		// that all of the subresources are in the correct state. The correct state
		// will be the main resource state.
		if (!mSubResourcesStates.empty())
		{
			auto rPlat = (dx12::RenderPlatform*)renderPlatform;
			for (int l = startLayer; l < numLayers; l++)
			{
				for (int m = startMip; m < numMips; m++)
				{
					auto curState = mSubResourcesStates[l][m];
					if (curState != mResourceState)
					{
						rPlat->ResourceTransitionSimple(deviceContext, mTextureDefault, curState, mResourceState, false, GetSubresourceIndex(m, l));
						mSubResourcesStates[l][m] = mResourceState;
					}
				}
			}
			rPlat->FlushBarriers(deviceContext);
		}
		if (allLayers && allMips)
			split_layouts = false;
		return mResourceState;
	}
	// Return a subresource state
	int curMip		= (mip == -1) ? 0 : mip;
	int curLayer	= (index == -1)? 0 : index;
	if (mSubResourcesStates.empty()) //Temporary fixed - AJR
	{
		//SIMUL_CERR << "mSubResourcesStates.empty() = true. Returning mResourceState." << std::endl;
		return mResourceState;
	}
	return mSubResourcesStates[curLayer][curMip];
}

void Texture::SplitLayouts()
{
	split_layouts=true;
}

void Texture::AssumeLayout(D3D12_RESOURCE_STATES state)
{
#if PLATFORM_DEBUG_BARRIERS1
	const size_t MAX_NAME_LENGTH = 30;
	char namebuf[MAX_NAME_LENGTH];
	GetD3DName(mTextureDefault, namebuf, MAX_NAME_LENGTH);
	SIMUL_COUT<<namebuf<<" 0x"<<std::hex<<(unsigned long long)mTextureDefault<<" assumed as layout "<<dx12::RenderPlatform::D3D12ResourceStateToString(state).c_str()<<std::endl;
#endif
    int numLayers       = (int)mSubResourcesStates.size();
	mResourceState      = state;
	// And set all the subresources to that state
	// We understand that we transitioned ALL the resources
    if (!mSubResourcesStates.empty())
    {
        for (int l = 0; l < numLayers; l++)
	    {
	    	for (int m = 0; m < mips; m++)
	    	{
	    		mSubResourcesStates[l][m] = state;
	    	}
	    }
    }
	split_layouts=false;
}

void Texture::SwitchToContext(crossplatform::DeviceContext &deviceContext)
{
	if(deviceContext.deviceContextType!=crossplatform::DeviceContextType::COMPUTE)
		return;
	SetLayout(deviceContext,D3D12_RESOURCE_STATE_COMMON);
}

unsigned Texture::GetSubresourceIndex(int mip, int layer)
{
	// Requested the whole resource
	if (mip == -1 && layer == -1)
	{
		return D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}
	int curMip = (mip == -1) ? 0 : mip;
	int curLayer = (layer == -1) ? 0 : layer;
	int totalArray = arraySize;
	if (cubemap)
		totalArray *= 6;
	return D3D12CalcSubresource(curMip, curLayer, 0, mips, totalArray);
}

void Texture::SetLayout(crossplatform::DeviceContext &deviceContext,D3D12_RESOURCE_STATES state, int mip /*= -1*/, int index /*= -1*/,bool flush)
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	int curArray = cubemap ? arraySize * 6 : arraySize;

	// Set the resource state
	if ((mip == -1||mips<=1) && (index == -1||curArray<=1))
	{
		int numLayers       = (int)curArray;
		if (split_layouts)
		{
			// And set all the subresources to that state
			// We understand that we transitioned ALL the resources
			if (!mSubResourcesStates.empty())
			{
				for (int l = 0; l < numLayers; l++)
				{
					for (int m = 0; m < mips; m++)
					{
						if(mSubResourcesStates[l][m]!=state)
							rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, mSubResourcesStates[l][m], state, flush, GetSubresourceIndex(m,l));
						mSubResourcesStates[l][m] = state;
					}
				}
			}
			//rPlat->ResourceTransitionSimple(mTextureDefault, mResourceState, state, true, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
		else if(mResourceState!=state)
		{
			rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, mResourceState, state,flush, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			for (int l = 0; l < numLayers; l++)
			{
		    	for (int m = 0; m < mips; m++)
		    	{
		    		mSubResourcesStates[l][m] = state;
		    	}
			}
		}
		mResourceState      = state;
		split_layouts = false;
	}
	// Set a subresource state
	else
	{
		int mipRange=1,m0=mip;
		if(mip == -1)
		{
			mipRange=mips;
			m0=0;
		}
		int layerRange=1,l0=index;
		if(index == -1)
		{
			layerRange=curArray;
			l0=0;
		}
		if (mSubResourcesStates.empty())
		{
			mSubResourcesStates.push_back({ state });
		}
		for(int l=l0;l<l0+layerRange;l++)
		{
			for(int m=m0;m<m0+mipRange;m++)
			{
				D3D12_RESOURCE_STATES oldState=mResourceState;
				if(split_layouts)
					oldState=mSubResourcesStates[l][m];
				if(state!=oldState)
				{
					int resourceIndex=GetSubresourceIndex(m, l);
					rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, oldState, state, flush, resourceIndex);
				}

				mSubResourcesStates[l][m] = state;
			}
		}
		// Array Size == 1 && mips = 1 (it only has 1 sub resource)
		// the asSRV code will return the mainSRV!
		if (mip==-1 && index==-1)
		{
			mResourceState = state;
		}
		else if(mResourceState!=state)
			split_layouts=true;
	}
}

void Texture::RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext)
{
	if((crossplatform::ResourceState)mExternalLayout==crossplatform::ResourceState::UNKNOWN)
		return;
	auto rPlat		= (dx12::RenderPlatform*)(renderPlatform);
	SetLayout(deviceContext, mExternalLayout);
}

void Texture::CreateUploadResource(int slices)
{
	SAFE_RELEASE_LATER(mTextureUpload);
	D3D12_RESOURCE_DESC textureDesc = mTextureDefault->GetDesc();
	textureDesc.Alignment = 0;							// Let runtime decide
	textureDesc.DepthOrArraySize =slices;
	textureDesc.Width = width;
	textureDesc.Height = length;
	textureDesc.MipLevels = mips;
	DXGI_FORMAT dxgiFormat = dx12::RenderPlatform::ToDxgiFormat(pixelFormat,compressionFormat);
	textureDesc.Format = dxgiFormat;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let runtime decide
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;	// Use D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET to generate mips.
	int totalNum = slices;
	size_t numSub = totalNum * textureDesc.MipLevels;
	std::vector<UINT64> sliceSizes(numSub);
	UINT64 textureUploadBufferSize = 0;
	std::vector<UINT>   numRows(numSub);
	pLayouts.resize(numSub);
	renderPlatform->AsD3D12Device()->GetCopyableFootprints
	(
		&textureDesc,
		0, numSub, 0,
		pLayouts.data(), numRows.data(), sliceSizes.data(),
		&textureUploadBufferSize
	);
	HRESULT res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		SIMUL_PPV_ARGS(&mTextureUpload)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mTextureUpload, textureUploadBufferSize)
	std::wstring n = L"UPLOAD_";
	n += std::wstring(name.begin(), name.end());
	mTextureUpload->SetName(n.c_str());
}