
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

using namespace simul;
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
	mNumSamples(1)
{
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

void Texture::FreeRTVTables()
{
	for (auto& rtv : renderTargetViews)
		delete rtv.second;
	renderTargetViews.clear();
	mTextureRtHeap.Release();
}

void Texture::FreeDSVTables()
{
	for (auto& dsv : depthStencilViews)
		delete dsv.second;
	depthStencilViews.clear();
	mTextureDsHeap.Release();
}

void Texture::FreeSRVTables()
{
	for (auto& srv : shaderResourceViews)
		delete srv.second;
	shaderResourceViews.clear();
	mTextureSrvHeap.Release();
}

void Texture::FreeUAVTables()
{
	for (auto& uav : unorderedAccessViews)
		delete uav.second;
	unorderedAccessViews.clear();
	mTextureUavHeap.Release();
}


void Texture::InitStateTable(int l, int m)
{
	// Create state table, at this point, we expect that the state
	// has already being set !!!
	auto curState = mResourceState;//GetCurrentState(deviceContext);
	mSubResourcesStates.clear();
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
	FreeDSVTables();
	FreeUAVTables();
	FreeSRVTables();
	ClearLoadingData();
	// Set the pointer to an invalid value so we can perform checks

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
	dim						= 2;
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
	if(textureLoadComplete)
		return;
	if(mips<0|| mips>16)
		mips=1;
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

	renderTarget = true;

	// Clear resources
	renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
	renderPlatformDx12->PushToReleaseManager(mTextureUpload, "mTextureUpload");
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
		}
		num_loaded++;
	}
	if(num_loaded)
	{
	// Create an upload heap
	// only single mip in the upload.
	textureDesc.MipLevels=1;
	UINT64 sliceSize [16];
	UINT64 textureUploadBufferSize = 0;
	UINT   numRows[16];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT pLayouts[16];
	renderPlatform->AsD3D12Device()->GetCopyableFootprints
	(
		&textureDesc,
		0,textureDesc.DepthOrArraySize,0,
		pLayouts, numRows, sliceSize,
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
	// Init states
	InitStateTable(arraySize, mips);
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);

	SIMUL_GPU_TRACK_MEMORY(mTextureUpload, textureUploadBufferSize)
	SIMUL_GPU_TRACK_MEMORY(mTextureDefault, textureUploadBufferSize)
	std::wstring n = L"UPLOAD_";
	n+= std::wstring(name.begin(), name.end());
	mTextureUpload->SetName(n.c_str());
	
	std::vector<D3D12_SUBRESOURCE_DATA> textureSubDatas;
	textureSubDatas.resize(arraySize);
	for(int i=0;i<wicContents.size();i++)
	{
		SetLayout(deviceContext, D3D12_RESOURCE_STATE_COPY_DEST, { 0, 1, i, 1 });
		WicContents &wic=wicContents[i];
		// Perform the texture copy
		D3D12_SUBRESOURCE_DATA &textureSubData	= textureSubDatas[i];
		textureSubData.pData					= wic.image->pixels; 
		textureSubData.RowPitch					= wic.image->rowPitch; 
		textureSubData.SlicePitch				= wic.image->rowPitch * textureDesc.Height;
		
		UpdateSubresources(deviceContext.asD3D12Context(), mTextureDefault, mTextureUpload,pLayouts[i].Offset, i*mips, 1, &textureSubDatas[i]);
	}
	
	}
	textureLoadComplete=true;
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
	dim				= 2;
	renderTarget	= true;
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

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView, bool setState, bool pixelShader)
{
	if(!textureLoadComplete)
	{
		FinishLoading(deviceContext);
		textureLoadComplete=true;
	}

	if (!ValidateTextureView(textureView))
		return nullptr;

	// Ensure a valid state for the resource
	if (setState)
	{
		auto curState = GetCurrentState(deviceContext,textureView.subresourceRange);
		auto newState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		newState |= pixelShader ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		if	((curState & (newState)) != (newState) )
		{
			SetLayout(deviceContext,newState,textureView.subresourceRange);
		}
	}

	uint64_t hash = textureView.GetHash();
	if (shaderResourceViews.find(hash) != shaderResourceViews.end())
		return shaderResourceViews[hash];

	D3D12_CPU_DESCRIPTOR_HANDLE* srv = new D3D12_CPU_DESCRIPTOR_HANDLE();
	if (mTextureSrvHeap.GetCount() == 0)
	{
		//https://math.stackexchange.com/questions/1656686/how-many-rectangles-can-be-observed-in-the-grid
		//https://math.stackexchange.com/questions/1240264/how-many-rectangles-or-triangles
		int layers = NumFaces();
		UINT totalCount = (mips * mips + mips) / 2 * (layers * layers + layers) / 2;
		totalCount += (mips + layers); //For aliasing -1 ranges for mips and layers.
		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, totalCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (name + " SrvHeap").c_str(), false);
	}

	const UINT& baseMipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& mipLevelCount = textureView.subresourceRange.mipLevelCount == -1 ? mips - baseMipLevel : textureView.subresourceRange.mipLevelCount;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	DXGI_FORMAT srvFormat = RenderPlatform::TypelessToSrvFormat(dxgi_format);
	RenderPlatform::DepthFormatToResourceAndSrvFormats(srvFormat, srvDesc.Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture1D.MipLevels = mipLevelCount;
		srvDesc.Texture1D.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		srvDesc.Texture1DArray.MostDetailedMip = baseMipLevel;
		srvDesc.Texture1DArray.MipLevels = mipLevelCount;
		srvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture1DArray.ArraySize = arrayLayerCount;
		srvDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture2D.MipLevels = mipLevelCount;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MostDetailedMip = baseMipLevel;
		srvDesc.Texture2DArray.MipLevels = mipLevelCount;
		srvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture2DArray.ArraySize = arrayLayerCount;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		srvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		srvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;

	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_3D)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip = baseMipLevel;
		srvDesc.Texture3D.MipLevels = mipLevelCount;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_CUBE)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = baseMipLevel;
		srvDesc.TextureCube.MipLevels = mipLevelCount;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_CUBE_ARRAY)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.MostDetailedMip = baseMipLevel;
		srvDesc.TextureCubeArray.MipLevels = mipLevelCount;
		srvDesc.TextureCubeArray.First2DArrayFace = 0;
		srvDesc.TextureCubeArray.NumCubes = NumFaces() / 6;
		srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");
	}

	renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
	*srv = mTextureSrvHeap.CpuHandle();
	shaderResourceViews[hash] = srv;
	mTextureSrvHeap.Offset();
	return srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!computable)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support UnorderedAccessViews.");
		return nullptr;
	}

	//Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext, textureView.subresourceRange);
	if ((curState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		SetLayout(deviceContext, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, textureView.subresourceRange);
	}

	uint64_t hash = textureView.GetHash();
	if (unorderedAccessViews.find(hash) != unorderedAccessViews.end())
		return unorderedAccessViews[hash];

	D3D12_CPU_DESCRIPTOR_HANDLE* uav = new D3D12_CPU_DESCRIPTOR_HANDLE();
	if (mTextureUavHeap.GetCount() == 0)
	{
		//https://math.stackexchange.com/questions/1656686/how-many-rectangles-can-be-observed-in-the-grid
		//https://math.stackexchange.com/questions/1240264/how-many-rectangles-or-triangles
		int layers = NumFaces();
		UINT totalCount = (mips * mips + mips) / 2 * (layers * layers + layers) / 2;
		totalCount += (mips + layers); //For aliasing -1 ranges for mips and layers.
		mTextureUavHeap.Restore((dx12::RenderPlatform*)renderPlatform, totalCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (name + " UavHeap").c_str(), false);
	}

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = RenderPlatform::TypelessToSrvFormat(dxgi_format);
	if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_1D)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		uavDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_1D_ARRAY)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		uavDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		uavDesc.Texture1DArray.ArraySize = arrayLayerCount;
		uavDesc.Texture1DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_2D)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipLevel;
		uavDesc.Texture2D.PlaneSlice = 0;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_2D_ARRAY)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		uavDesc.Texture2DArray.ArraySize = arrayLayerCount;
		uavDesc.Texture2DArray.MipSlice = mipLevel;
		uavDesc.Texture2DArray.PlaneSlice = 0;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::RW_TEXTURE_3D)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = depth >> mipLevel;
		uavDesc.Texture3D.MipSlice = mipLevel;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unsupported crossplatform::ShaderResourceType.");
	}
	renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
	*uav = mTextureUavHeap.CpuHandle();
	unorderedAccessViews[hash] = uav;
	mTextureUavHeap.Offset();
	return uav;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12DepthStencilView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!depthStencil)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support DepthStencilViews.");
		return nullptr;
	}

	// Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext, textureView.subresourceRange);
	if ((curState & D3D12_RESOURCE_STATE_DEPTH_WRITE) != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		SetLayout(deviceContext, D3D12_RESOURCE_STATE_DEPTH_WRITE, textureView.subresourceRange);
	}

	uint64_t hash = textureView.GetHash();
	if (depthStencilViews.find(hash) != depthStencilViews.end())
		return depthStencilViews[hash];
	
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = new D3D12_CPU_DESCRIPTOR_HANDLE();
	if (mTextureDsHeap.GetCount() == 0)
	{
		//https://math.stackexchange.com/questions/1656686/how-many-rectangles-can-be-observed-in-the-grid
		//https://math.stackexchange.com/questions/1240264/how-many-rectangles-or-triangles
		int layers = NumFaces();
		UINT totalCount = (mips * mips + mips) / 2 * (layers * layers + layers) / 2;
		totalCount += (mips + layers); //For aliasing -1 ranges for mips and layers.
		mTextureDsHeap.Restore((dx12::RenderPlatform*)renderPlatform, totalCount, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, (name + " DsvHeap").c_str(), false);
	}

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = RenderPlatform::TypelessToDsvFormat(dxgi_format);
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		dsvDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		dsvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture1DArray.ArraySize = arrayLayerCount;
		dsvDesc.Texture1DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture2DArray.ArraySize = arrayLayerCount;
		dsvDesc.Texture2DArray.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		dsvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		dsvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unknown crossplatform::ShaderResourceType.");
	}

	renderPlatform->AsD3D12Device()->CreateDepthStencilView(mTextureDefault, &dsvDesc, mTextureDsHeap.CpuHandle());
	*dsv = mTextureDsHeap.CpuHandle();
	depthStencilViews[hash] = dsv;
	mTextureDsHeap.Offset();
	return dsv;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12RenderTargetView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView)
{
	if (!ValidateTextureView(textureView))
		return nullptr;

	if (!renderTarget)
	{
		SIMUL_BREAK_ONCE("Texture doesn't support RenderTargetViews.");
		return nullptr;
	}

	// Ensure a valid state for the resource
	auto curState = GetCurrentState(deviceContext, textureView.subresourceRange);
	if ((curState & D3D12_RESOURCE_STATE_RENDER_TARGET) != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		SetLayout(deviceContext, D3D12_RESOURCE_STATE_RENDER_TARGET, textureView.subresourceRange);
	}

	uint64_t hash = textureView.GetHash();
	if (renderTargetViews.find(hash) != renderTargetViews.end())
		return renderTargetViews[hash];

	D3D12_CPU_DESCRIPTOR_HANDLE* rtv = new D3D12_CPU_DESCRIPTOR_HANDLE();
	if (mTextureRtHeap.GetCount() == 0)
	{
		//https://math.stackexchange.com/questions/1656686/how-many-rectangles-can-be-observed-in-the-grid
		//https://math.stackexchange.com/questions/1240264/how-many-rectangles-or-triangles
		int layers = NumFaces();
		UINT totalCount = (mips * mips + mips) / 2 * (layers * layers + layers) / 2;
		totalCount += (mips + layers); //For aliasing -1 ranges for mips and layers.
		mTextureRtHeap.Restore((dx12::RenderPlatform*)renderPlatform, totalCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, (name + " RtHeap").c_str(), false);
	}

	const UINT& mipLevel = textureView.subresourceRange.baseMipLevel;
	const UINT& baseArrayLayer = textureView.subresourceRange.baseArrayLayer;
	const UINT& arrayLayerCount = textureView.subresourceRange.arrayLayerCount == -1 ? NumFaces() - baseArrayLayer : textureView.subresourceRange.arrayLayerCount;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = RenderPlatform::TypelessToSrvFormat(dxgi_format);
	if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		rtvDesc.Texture1D.MipSlice = mipLevel;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_1D_ARRAY)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		rtvDesc.Texture1DArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture1DArray.ArraySize = arrayLayerCount;
		rtvDesc.Texture1DArray.MipSlice = mipLevel;
}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = mipLevel;
		rtvDesc.Texture2D.PlaneSlice = 0;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture2DArray.ArraySize = arrayLayerCount;
		rtvDesc.Texture2DArray.MipSlice = mipLevel;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_2DMS_ARRAY)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		rtvDesc.Texture2DMSArray.FirstArraySlice = baseArrayLayer;
		rtvDesc.Texture2DMSArray.ArraySize = arrayLayerCount;
	}
	else if (textureView.type == crossplatform::ShaderResourceType::TEXTURE_3D)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.FirstWSlice = 0;
		rtvDesc.Texture3D.WSize = depth >> mipLevel;
		rtvDesc.Texture3D.MipSlice = mipLevel;
	}
	else
	{
		SIMUL_BREAK_ONCE("Unknown crossplatform::ShaderResourceType.");
	}

	renderPlatform->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtvDesc, mTextureRtHeap.CpuHandle());
	*rtv = mTextureRtHeap.CpuHandle();
	renderTargetViews[hash] = rtv;
	mTextureRtHeap.Offset();
	return rtv;
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
	return computable;
}

bool Texture::HasRenderTargets() const
{
	return renderTarget;
}

bool Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,int w,int l,crossplatform::PixelFormat f,bool make_rt, bool setDepthStencil, int numOfSamples)
{
	return InitFromExternalD3D12Texture2D(renderPlatform,(ID3D12Resource*)t,make_rt,setDepthStencil);
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

bool Texture::InitFromExternalD3D12Texture2D(crossplatform::RenderPlatform* r, ID3D12Resource * t, bool make_rt, bool setDepthStencil)
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
	
	mExternalLayout=D3D12_RESOURCE_STATE_COMMON;
	renderPlatform = r;
	
	// If it's the same as before, return.
	if (mTextureDefault == t)
	{
		return true;
	}
	// If it's the same texture, and we created our own srv, that's fine, return.
	if (mTextureDefault != NULL && mTextureDefault == t)
	{
		return true;
	}
	if (mTextureDefault)
	{
		if(!mInitializedFromExternal)
		{
			auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
			renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
		}
		mTextureDefault=nullptr;
	}
	if (!t)
	{
		return false;
	}

	t->AddRef();
	FreeSRVTables();
	mTextureDefault				= t;
	mInitializedFromExternal	= true;
	// Textures initialized from external should be passed by as a SRV so we expect
	// that the resource was previously transitioned to GENERIC_READ

	if (mTextureDefault)
	{
		D3D12_RESOURCE_DESC textureDesc = mTextureDefault->GetDesc();
		InitStateTable(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
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
		width		= (int)textureDesc.Width;
		length		= (int)textureDesc.Height;
		mNumSamples = textureDesc.SampleDesc.Count;
		if ((textureDesc.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)==0)
		{
			arraySize	= textureDesc.DepthOrArraySize;
			mips		= textureDesc.MipLevels;
		}
		depth       = textureDesc.DepthOrArraySize;

		// Create render target views for this external texture:
		if (make_rt && !setDepthStencil)
		{
			if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
			{
				FreeRTVTables();
				this->renderTarget = true;
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
				FreeDSVTables();
				this->depthStencil = true;
			}
			else
			{
				SIMUL_CERR << "This external texture can not be created as a render target. The flags do not support it.\n";
				return false;
			}
		}
	}
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	dim			= 2;
	return true;
}

bool Texture::InitFromExternalTexture3D(crossplatform::RenderPlatform *r,void *ta,bool make_uav)
{
	return false;
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int d,crossplatform::PixelFormat pf,bool computable,int m,bool rendertargets)
{
	HRESULT res = S_FALSE;

	pixelFormat		= pf;
	DXGI_FORMAT dxgiFormat	= dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT srvFormat	=dxgiFormat;
	DXGI_FORMAT uavFormat	=dxgiFormat;
	if(dxgiFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		srvFormat	=dxgiFormat;
		dxgiFormat	=DXGI_FORMAT_R8G8B8A8_TYPELESS;
		uavFormat	=DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	dim				= 3;
	renderPlatform  = r;
	bool ok			= true;

	if (mTextureDefault)
	{
		D3D12_RESOURCE_DESC desc = mTextureDefault->GetDesc();
		if (desc.Width != w || desc.Height != l || desc.DepthOrArraySize != d || desc.MipLevels != m || desc.Format != dxgiFormat)
		{
			int mindim=std::min(std::min(w,l),d);
			while(m>1&&(1<<(m-1))>mindim)
			{
				m--;
			}
			if (desc.Width != w || desc.Height != l || desc.DepthOrArraySize != d || desc.MipLevels != m || desc.Format != dxgiFormat)
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
		int mindim=std::min(std::min(w,l),d);
		while(m>1&&(1<<(m-1))>mindim)
		{
			m--;
		}
		auto &deviceContext=renderPlatform->GetImmediateContext();
		dxgi_format = dxgiFormat;
		width		= w;
		length		= l;
		depth		= d;

		SIMUL_ASSERT(w > 0 && l > 0 && w <= 65536 && l <= 65536);
		InvalidateDeviceObjects();

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
			clearValues.Format		= dxgiFormat;
			clearValues.Color[0]	= 1.0f;
			clearValues.Color[1]	= 0.0f;
			clearValues.Color[2]	= 0.0f;
			clearValues.Color[3]	= 1.0f;
		}

		mips = m;
		arraySize = 1;

		// Find the initial texture state
		InitStateTable(1, m);
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
		if (rendertargets)
			AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (computable)
			AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Clean resources
		SAFE_RELEASE_LATER(mTextureDefault);


		res = r->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			GetCurrentState(deviceContext),
			rendertargets? &clearValues : nullptr,
			SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l * d * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());

		// Create the main SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= dxgiFormat;
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels				= m;
		srvDesc.Texture3D.MostDetailedMip		= 0;
		FreeSRVTables();

		if (name.length() == 0)
			SIMUL_BREAK_INTERNAL("Unnamed texture")

		if (computable)
		{
			this->computable = true;
			FreeUAVTables();
		}
		if (rendertargets)
		{
			this->renderTarget = true;
			FreeRTVTables();
		}
		if (d <= 8 && rendertargets && (!ok))
		{
			SIMUL_BREAK_INTERNAL("Render targets for 3D textures are not currently supported.");
		}
	}
	
	return changed;
}

bool Texture::EnsureTexture(crossplatform::RenderPlatform *r,crossplatform::TextureCreate *tc)
{
	bool res = false;
	if (tc->d == 1 && tc->arraysize == 1)
		res = ensureTexture2DSizeAndFormat(r, tc->w, tc->l, tc->mips, tc->f, tc->computable, tc->make_rt, tc->setDepthStencil, tc->numOfSamples, tc->aa_quality, false, tc->clear, tc->clearDepth, tc->clearStencil);
	else if (tc->d == 1)
		res = ensureTextureArraySizeAndFormat(r, tc->w, tc->l, tc->arraysize, tc->mips, tc->f, tc->computable, tc->make_rt, tc->cubemap);
	else
		res = ensureTexture3DSizeAndFormat(r, tc->w, tc->l, tc->d, tc->f, tc->computable, tc->mips, tc->make_rt);
	return res;
}

bool Texture::ensureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l, int m,
											crossplatform::PixelFormat f,
											bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil)
{
	return EnsureTexture2DSizeAndFormat(r,w,l,m,f,computable,rendertarget,depthstencil,num_samples,aa_quality,wrap,clear,clearDepth,clearStencil);
}

bool Texture::EnsureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l, int m,
											crossplatform::PixelFormat f,
											bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil
											,crossplatform::CompressionFormat cf,const void *data)
{
	// Define pixel formats of this texture
	renderPlatform	= r;
	pixelFormat		= f;
	dxgi_format		= (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT texture2dFormat = dxgi_format;
	DXGI_FORMAT srvFormat		= dxgi_format;
	if (texture2dFormat == DXGI_FORMAT_D32_FLOAT)
	{
		texture2dFormat = DXGI_FORMAT_R32_TYPELESS;
		srvFormat		= DXGI_FORMAT_R32_FLOAT;
	}
	if (texture2dFormat == DXGI_FORMAT_D16_UNORM)
	{
		texture2dFormat = DXGI_FORMAT_R16_TYPELESS;
		srvFormat		= DXGI_FORMAT_R16_UNORM;
	}
	if (texture2dFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		texture2dFormat = DXGI_FORMAT_R24G8_TYPELESS;
		srvFormat		= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	dim			= 2;
	HRESULT res = S_FALSE;
	
	bool ok = true;
	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
		if (desc.Width != w || desc.Height != l || desc.MipLevels != m || desc.Format != texture2dFormat)
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
	}
	else
	{
		ok = false;
	}

	if (!ok)
	{
		if (w*l <= 0)
			return false;
		auto &deviceContext=renderPlatform->GetImmediateContext();
		width = w;
		length = l;

		InvalidateDeviceObjects();
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
			texture2dFormat,
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
			clearValues.Format = texture2dFormat;
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
		renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
		mTextureDefault = nullptr;


		// Create the texture resource
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			(rendertarget || depthstencil) ? &clearValues : nullptr,
			SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l  * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());
		
		InitStateTable(1, m);
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
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

		if (computable)
		{
			this->computable = true;
			FreeUAVTables();
		}
		if (rendertarget)
		{
			this->renderTarget = true;
			FreeRTVTables();
		}
		if (depthstencil)
		{
			this->depthStencil = true;
			FreeDSVTables();
		}
	}

	mips		= m;
	arraySize	= 1;
	
	return !ok;
}

bool Texture::ensureVideoTexture(crossplatform::RenderPlatform* r, int w, int l, crossplatform::PixelFormat f, crossplatform::VideoTextureType texType)
{
#if SIMUL_D3D12_VIDEO_SUPPORTED
	// Define pixel formats of this texture
	renderPlatform = r;
	pixelFormat = f;
	dxgi_format = (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT texture2dFormat = dxgi_format;
	if (texture2dFormat == DXGI_FORMAT_D32_FLOAT)
	{
		texture2dFormat = DXGI_FORMAT_R32_TYPELESS;
	}
	if (texture2dFormat == DXGI_FORMAT_D16_UNORM)
	{
		texture2dFormat = DXGI_FORMAT_R16_TYPELESS;
	}
	if (texture2dFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		texture2dFormat = DXGI_FORMAT_R24G8_TYPELESS;
	}
	dim = 2;
	HRESULT res = S_FALSE;

	int mipLevels = 1;

	bool ok = true;
	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
		if (desc.Width != w || desc.Height != l || desc.MipLevels != mipLevels || desc.Format != texture2dFormat)
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
		auto& deviceContext = renderPlatform->GetImmediateContext();
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
			texture2dFormat,
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
		renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
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

		InitStateTable(1, mipLevels);	
		AssumeLayout(initialState);
	}

	mips = 1;
	arraySize = 1;
	return true;
#else
	return false;
#endif
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int num,int m,crossplatform::PixelFormat f,bool computable,bool rendertarget,bool cubemap)
{
	bool ok			= true;
	int totalNum	= cubemap ? 6 * num : num;

	if (mTextureDefault)
	{
		auto tDesc = mTextureDefault->GetDesc();
		if (tDesc.DepthOrArraySize != totalNum || tDesc.MipLevels != m || tDesc.Width != w || tDesc.Height != l || tDesc.Format != dxgi_format)
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
	}
	else
	{
		ok = false;
	}
	if (ok)
	{
		return false;
	}

	HRESULT res		= S_OK;
	renderPlatform	= r;
	pixelFormat		= f;
	dxgi_format		= dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	width			= w;
	length			= l;
	depth			= 1;
	dim				= 2;
	mips            = m;
	arraySize       = num;
	this->computable = computable;
	this->cubemap   = cubemap;

	D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
	textureFlags = (D3D12_RESOURCE_FLAGS)
	(
		textureFlags |
		(computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : 0) |
		(rendertarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : 0)
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
	D3D12_CLEAR_VALUE clearValues = {};
	if (rendertarget)
	{
		clearValues.Format = dxgi_format;
		clearValues.Color[0] = 0.0f;
		clearValues.Color[1] = 0.0f;
		clearValues.Color[2] = 0.0f;
		clearValues.Color[3] = 0.0f;
	}

	// Clear resources
	SAFE_RELEASE_LATER(mTextureDefault);
	mResourceState=D3D12_RESOURCE_STATE_GENERIC_READ;
	res = r->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		mResourceState,
		rendertarget? &clearValues : nullptr,
		SIMUL_PPV_ARGS(&mTextureDefault)
	);
	SIMUL_ASSERT(res == S_OK);
	size_t texSize = w * l * totalNum * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
	SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
	std::wstring n = L"GPU_";
	n += std::wstring(name.begin(), name.end());
	mTextureDefault->SetName(n.c_str());

	FreeSRVTables();
	FreeUAVTables();
	FreeRTVTables();
	FreeDSVTables();
	
	InitStateTable(totalNum, m);
	// Find the initial texture state
	AssumeLayout(mResourceState);
	// We have SET the state. So mResourceState is the real value.
  /*  if (rendertarget)
	{
		AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	if (computable)
	{
		AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}*/
	

	if (rendertarget)
	{
		this->renderTarget = true;
	}

	auto rPlat		= (dx12::RenderPlatform*)renderPlatform;

	return true;
}

void Texture::ensureTexture1DSizeAndFormat(ID3D12Device* pd3dDevice,int w,crossplatform::PixelFormat pf,bool computable)
{
}

void Texture::ClearDepthStencil(crossplatform::GraphicsDeviceContext& deviceContext, float depthClear, int stencilClear)
{
	const D3D12_CLEAR_FLAGS kFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = AsD3D12DepthStencilView(deviceContext, { GetShaderResourceTypeForRTVAndDSV(), {} });
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

void Texture::activateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::TextureView textureView)
{
	const int& mip = textureView.subresourceRange.baseMipLevel;
	const int& array_index = textureView.subresourceRange.baseArrayLayer;
	if (textureView.type == crossplatform::ShaderResourceType::UNKNOWN)
		textureView.type = GetShaderResourceTypeForRTVAndDSV();

	SetLayout(deviceContext, D3D12_RESOURCE_STATE_RENDER_TARGET, textureView.subresourceRange);
	if (renderTarget)
	{
		auto rp		= (dx12::RenderPlatform*)deviceContext.renderPlatform;
		rp->FlushBarriers(deviceContext);
		auto rtView = AsD3D12RenderTargetView(deviceContext, textureView);

		targetsAndViewport.num				= 1;
		targetsAndViewport.m_dt				= nullptr;
		targetsAndViewport.m_rt[0]			= rtView;
		targetsAndViewport.rtFormats[0]     = pixelFormat;
		targetsAndViewport.viewport.w		= (int)(std::max(1, (width >> mip)));
		targetsAndViewport.viewport.h		= (int)(std::max(1, (length >> mip)));
		targetsAndViewport.viewport.x		= 0;
		targetsAndViewport.viewport.y		= 0;

		rp->ActivateRenderTargets(deviceContext, &targetsAndViewport);
	}
}

void Texture::deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext)
{
	auto rp = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	rp->DeactivateRenderTargets(deviceContext);
}

int Texture::GetSampleCount()const
{
	return mNumSamples == 1 ? 0 : mNumSamples;
}

bool Texture::AreSubresourcesInSameState(const crossplatform::SubresourceRange& subresourceRange)
{
	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;

	std::vector<D3D12_RESOURCE_STATES> stateCheck;
	for (uint32_t layer = startLayer; layer < startLayer + numLayers; layer++)
		for (uint32_t mip = startMip; mip < startMip + numMips; mip++)
			stateCheck.push_back(mSubResourcesStates[layer][mip]);

	return std::equal(stateCheck.begin() + 1, stateCheck.end(), stateCheck.begin());
}

D3D12_RESOURCE_STATES Texture::GetCurrentState(crossplatform::DeviceContext& deviceContext, const crossplatform::SubresourceRange& subresourceRange)
{
	if (mSubResourcesStates.empty())
		return mResourceState;

	if (AreSubresourcesInSameState({}))
		return mResourceState;

	if (AreSubresourcesInSameState(subresourceRange))
		return mSubResourcesStates[subresourceRange.baseArrayLayer][subresourceRange.baseMipLevel];

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;


	// Return the resource state of a mip or array layer, or the whole resource
	if (numMips > 1 || numLayers > 1)
	{
		// If we request the state of the ranges of subresource, we have to make sure
		// that all of the subresources are in the correct state. The correct state
		// will be the main resource state.
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
		for (uint32_t l = startLayer; l < startLayer + numLayers; l++)
		{
			for (uint32_t m = startMip; m < startMip + numMips; m++)
			{
				auto curState = mSubResourcesStates[l][m];
				if (curState != mResourceState)
				{
					rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, curState, mResourceState, false, GetSubresourceIndex(m,l));
					mSubResourcesStates[l][m] = mResourceState;
				}
			}
		}
		rPlat->FlushBarriers(deviceContext);
		return mResourceState;
	}

	// Return a subresource state
	return mSubResourcesStates[startLayer][startMip];
}

void Texture::SetLayout(crossplatform::DeviceContext &deviceContext, D3D12_RESOURCE_STATES state, const crossplatform::SubresourceRange& subresourceRange)
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	int curArray = cubemap ? arraySize * 6 : arraySize;
	const bool split_layouts = !AreSubresourcesInSameState({});

	const uint32_t& startMip = subresourceRange.baseMipLevel;
	const uint32_t& numMips = subresourceRange.mipLevelCount == -1 ? mips - startMip : subresourceRange.mipLevelCount;
	const uint32_t& startLayer = subresourceRange.baseArrayLayer;
	const uint32_t& numLayers = subresourceRange.arrayLayerCount == -1 ? NumFaces() - startLayer : subresourceRange.arrayLayerCount;
	
	bool allSubresources = ((startMip == 0) && (startLayer == 0)) && ((numMips == mips) && (numLayers == curArray));

	// Set the whole resource state
	if (allSubresources)
	{
		int numLayers = (int)curArray;
		if (split_layouts)
		{
			// And set all the subresources to that state
			// We understand that we transitioned ALL the resources
			for (int l = 0; l < numLayers; l++)
			{
				for (int m = 0; m < mips; m++)
				{
					if(mSubResourcesStates[l][m]!=state)
						rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, mSubResourcesStates[l][m], state, false, GetSubresourceIndex(m,l));
					mSubResourcesStates[l][m] = state;
				}
			}
		}
		else if(mResourceState!=state)
		{
			rPlat->ResourceTransitionSimple(deviceContext,mTextureDefault, mResourceState, state, false, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			for (int l = 0; l < numLayers; l++)
			{
				for (int m = 0; m < mips; m++)
				{
					mSubResourcesStates[l][m] = state;
				}
			}
		}
		mResourceState = state;
	}
	// Set a subresource range states
	else
	{
		for (uint32_t l = startLayer; l < startLayer + numLayers; l++)
		{
			for (uint32_t m = startMip; m < startMip + numMips; m++)
			{
				D3D12_RESOURCE_STATES oldState = mResourceState;
				if (split_layouts)
					oldState = mSubResourcesStates[l][m];
				if (state != oldState)
				{
					int resourceIndex = GetSubresourceIndex(m, l);
					rPlat->ResourceTransitionSimple(deviceContext, mTextureDefault, oldState, state, true, resourceIndex);
				}

				mSubResourcesStates[l][m] = state;
			}
		}
	}

	if (AreSubresourcesInSameState({}))
		mResourceState = state;
}

void Texture::AssumeLayout(D3D12_RESOURCE_STATES state)
{
#if SIMUL_DEBUG_BARRIERS
	SIMUL_COUT<<name.c_str()<<" 0x"<<std::hex<<(unsigned long long)mTextureDefault<<" assumed as layout "<<dx12::RenderPlatform::D3D12ResourceStateToString(state).c_str()<<std::endl;
#endif
	mResourceState	= state;
	// And set all the subresources to that state
	// We understand that we transitioned ALL the resources
	if (!mSubResourcesStates.empty())
	{
		int numLayers = (int)mSubResourcesStates.size();
		for (int l = 0; l < numLayers; l++)
		{
			for (int m = 0; m < mips; m++)
			{
				mSubResourcesStates[l][m] = state;
			}
		}
	}
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

void Texture::SwitchToContext(crossplatform::DeviceContext &deviceContext)
{
	if(deviceContext.deviceContextType!=crossplatform::DeviceContextType::COMPUTE)
		return;
	SetLayout(deviceContext,D3D12_RESOURCE_STATE_COMMON);
}

void Texture::RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext)
{
	if((crossplatform::ResourceState)mExternalLayout==crossplatform::ResourceState::UNKNOWN)
		return;
	auto rPlat		= (dx12::RenderPlatform*)(renderPlatform);
	SetLayout(deviceContext, mExternalLayout);
}

