
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX12/GpuProfiler.h"
#include "Platform/DirectX12/ConstantBuffer.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/PlatformStructuredBuffer.h"
#include "Platform/DirectX12/BaseAccelerationStructure.h"
#include "Platform/DirectX12/TopLevelAccelerationStructure.h"
#include "Platform/DirectX12/BottomLevelAccelerationStructure.h"
#include "Platform/DirectX12/ShaderBindingTable.h"
#include "Platform/DirectX12/Texture.h"
#include "Platform/DirectX12/Framebuffer.h"
#include "Platform/DirectX12/Effect.h"
#include "Platform/DirectX12/Buffer.h"
#include "Platform/DirectX12/Layout.h"
#include "Platform/DirectX12/Heap.h"
#include "DisplaySurface.h"
#include <algorithm>
#ifdef SIMUL_ENABLE_PIX
    //#include "Platform/External/PIX/Include/pix3.h"
	//#pragma comment(lib, "WinPixEventRuntime.lib")
	static HMODULE hWinPixEventRuntime;
#endif
using namespace simul;
using namespace dx12;
#if SIMUL_INTERNAL_CHECKS
#define PLATFORM_D3D12_RELEASE_MANAGER_CHECKS 0
#define SIMUL_DEBUG_BARRIERS 0
crossplatform::DeviceContextType barrierDeviceContextType=crossplatform::DeviceContextType::GRAPHICS;
#else
#define PLATFORM_D3D12_RELEASE_MANAGER_CHECKS 0
#define SIMUL_DEBUG_BARRIERS 0
#endif

const char *PlatformD3D12GetErrorText(HRESULT hr)
{
	static std::string str;
#ifdef _XBOX_ONE
	std::wstring wstr;
	wstr.resize(101);
	DWORD res=FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM ,//| FORMAT_MESSAGE_IGNORE_INSERTS
			NULL, hr, 0, (wchar_t*)wstr.data(),100, NULL);
	str=base::WStringToUtf8(wstr);
#else
	char buf[101];
	buf[100] = 0;
	DWORD res = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM,//| FORMAT_MESSAGE_IGNORE_INSERTS
		NULL, hr, 0, buf, 100, NULL);
	str = buf;
#endif
	return str.c_str();
}
RenderPlatform::RenderPlatform():
    DepthStateOverride(nullptr)
    ,BlendStateOverride(nullptr)
    ,RasterStateOverride(nullptr)
	
	,mTimeStampFreq(0)
	,m12Device(nullptr)
	,m12Queue(nullptr)
	,mImmediateCommandList(nullptr)
	,mFrameHeap(nullptr)
    ,mFrameOverrideSamplerHeap(nullptr)
	,mSamplerHeap(nullptr) 
	,mRenderTargetHeap(nullptr)
	,mDepthStencilHeap(nullptr)
	,mNullHeap(nullptr)
	,mGRootSignature(nullptr)
	,mCRootSignature(nullptr)
	,mDummy2D(nullptr)
	,mDummy3D(nullptr)
	,mCurInputLayout(nullptr)
	,mIsMsaaEnabled(false)
{
	mMsaaInfo.Count = 1;
	mMsaaInfo.Quality = 0;

    mCurBarriers    = 0;
    mTotalBarriers  = 16; 
    mPendingBarriers.resize(mTotalBarriers);

#ifdef SIMUL_ENABLE_PIX
	if (hWinPixEventRuntime == 0)
		hWinPixEventRuntime = LoadLibraryA("../../Platform/External/PIX/lib/WinPixEventRuntime.dll");
#endif
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
#ifdef SIMUL_ENABLE_PIX
	if (hWinPixEventRuntime != 0)
	{
		if (!FreeLibrary(hWinPixEventRuntime))
		{
			SIMUL_BREAK_ONCE("D3D12: Failed to Free Library. File: WinPixEventRuntime.dll.")
		}
	}
#endif
}

float RenderPlatform::GetDefaultOutputGamma() const
{
	static float g=1.0f;
	return g;
}

void RenderPlatform::SetImmediateContext(ImmediateContext * ctx)
{
	mImmediateCommandList				= ctx->ICommandList;
	mImmediateAllocator					= ctx->IAllocator;
	immediateContext.platform_context	= mImmediateCommandList;
	immediateContext.renderPlatform		= this;
	bImmediateContextActive=ctx->bActive;
	bExternalImmediate=ctx->bActive;
}

void RenderPlatform::SetD3D12ComputeContext(D3D12ComputeContext * ctx)
{
	computeContext.platform_context		= ctx->ICommandList;
	computeContext.renderPlatform		= this;
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return mImmediateCommandList;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return m12Device;
}

#if !defined(_XBOX_ONE) && !defined(_GAMING_XBOX_XBOXONE)
ID3D12Device5* RenderPlatform::AsD3D12Device5()
{
	ID3D12Device5* m12Device5 = nullptr;
	HRESULT res = m12Device->QueryInterface(SIMUL_PPV_ARGS(&m12Device5));

	if(res != S_OK)
		m12Device5 = nullptr;

	return m12Device5;
}
#endif

std::string RenderPlatform::D3D12ResourceStateToString(D3D12_RESOURCE_STATES states)
{
	std::string str;

	if(states&D3D12_RESOURCE_STATE_COMMON)								str+=" COMMON";
	if(states&D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)			str+=" VERTEX_AND_CONSTANT_BUFFER";
	if(states&D3D12_RESOURCE_STATE_INDEX_BUFFER)						str+=" INDEX_BUFFER";
	if(states&D3D12_RESOURCE_STATE_RENDER_TARGET)						str+=" RENDER_TARGET";
	if(states&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)					str+=" UNORDERED_ACCESS";
	if(states&D3D12_RESOURCE_STATE_DEPTH_WRITE)							str+=" DEPTH_WRITE";
	if(states&D3D12_RESOURCE_STATE_DEPTH_READ)							str+=" DEPTH_READ";
	if(states&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)			str+=" NON_PIXEL_SHADER_RESOURCE";
	if(states&D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)				str+=" PIXEL_SHADER_RESOURCE";
	if(states&D3D12_RESOURCE_STATE_STREAM_OUT)							str+=" STREAM_OUT";
	if(states&D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)					str+=" INDIRECT_ARGUMENT";
	if(states&D3D12_RESOURCE_STATE_COPY_DEST)							str+=" COPY_DEST";
	if(states&D3D12_RESOURCE_STATE_COPY_SOURCE)							str+=" COPY_SOURCE";
	if(states&D3D12_RESOURCE_STATE_RESOLVE_DEST)						str+=" RESOLVE_DEST";
	if(states&D3D12_RESOURCE_STATE_RESOLVE_SOURCE)						str+=" RESOLVE_SOURCE";
	if(states&D3D12_RESOURCE_STATE_PRESENT)								str+=" PRESENT";
	if(states&D3D12_RESOURCE_STATE_PREDICATION)							str+=" PREDICATION";
#if !defined(_XBOX_ONE) && !defined(_GAMING_XBOX_XBOXONE)
	if(states&D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)	str += " RAYTRACING_ACCELERATION_STRUCTURE";
	if(states&D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE)					str += " SHADING_RATE_SOURCE";
	if(states&D3D12_RESOURCE_STATE_VIDEO_DECODE_READ)					str+=" VIDEO_DECODE_READ";
	if(states&D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE)					str+=" VIDEO_DECODE_WRITE";
	if(states&D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ)					str+=" VIDEO_PROCESS_READ";
	if(states&D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE)					str+=" VIDEO_PROCESS_WRITE";
	if(states&D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ)					str+=" VIDEO_ENCODE_READ";
	if(states&D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)					str+=" VIDEO_ENCODE_WRITE";
#endif
	if(D3D12_RESOURCE_STATE_GENERIC_READ==states)						str=" GENERIC_READ";
	if((D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)==states)						str=" SHADER_RESOURCE";
	if(str.length()==0)
	{
		SIMUL_BREAK("Bad resource state");
	}
	return str;
}
#include <iomanip>

void RenderPlatform::ResourceTransitionSimple(crossplatform::DeviceContext& deviceContext,	ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, 
												bool flush /*= false*/, UINT subRes /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
#ifndef DISABLE_BARRIERS
	// merge barriers??
	bool found=false;
	for(int i=0;i<mCurBarriers;i++)
	{
		auto& b = mPendingBarriers[i];
		if(b.Transition.pResource==res&&b.Transition.Subresource==subRes)
		{
			SIMUL_ASSERT(before==b.Transition.StateAfter);
			if(before!=b.Transition.StateAfter)
			{
#if SIMUL_DEBUG_BARRIERS
				if(deviceContext.deviceContextType==barrierDeviceContextType)
				{
					SIMUL_CERR<<"Barrier error : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<"("<<subRes<<") - "<<D3D12ResourceStateToString(before)<<" IS NOT "<<D3D12ResourceStateToString(b.Transition.StateBefore)<<std::endl;
				}
#endif
			}
			if(after==b.Transition.StateBefore)
			{
				if(mCurBarriers>1)
				{
					std::swap(b,mPendingBarriers[mCurBarriers-1]);
#if SIMUL_DEBUG_BARRIERS
				if(deviceContext.deviceContextType==barrierDeviceContextType)
				{
					SIMUL_COUT<<"Barrier swapped: 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<"("<<subRes<<") from "<<D3D12ResourceStateToString(before)<<" to "<<D3D12ResourceStateToString(after)<<std::endl;
				}
#endif
				}
				else
				{
#if SIMUL_DEBUG_BARRIERS
				if(deviceContext.deviceContextType==barrierDeviceContextType)
				{
					SIMUL_COUT<<"Barrier removed : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<"("<<subRes<<") from "<<D3D12ResourceStateToString(before)<<" to "<<D3D12ResourceStateToString(after)<<std::endl;
				}
#endif
				}
				mCurBarriers--;
			}
			else
			{
#if SIMUL_DEBUG_BARRIERS
				if(deviceContext.deviceContextType==barrierDeviceContextType)
				{
					SIMUL_COUT<<"Barrier combined : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<"("<<subRes<<") from "<<D3D12ResourceStateToString(before)<<" to "<<D3D12ResourceStateToString(after)<<std::endl;
				}
#endif
				b.Transition.StateAfter=after;
			}
			found=true;
		}
	}
	if(!found)
	{
		auto& barrier = mPendingBarriers[mCurBarriers++];
#if SIMUL_DEBUG_BARRIERS
		if(deviceContext.deviceContextType==barrierDeviceContextType)
		{
			SIMUL_COUT<<"Barrier : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<"("<<subRes<<") from "<<D3D12ResourceStateToString(before)<<" to "<<D3D12ResourceStateToString(after)<<std::endl;
		}
#endif
		barrier = CD3DX12_RESOURCE_BARRIER::Transition
		(
			res, before, after, subRes
		);
	}
	//if (flush)
	{
		FlushBarriers(deviceContext);
	}
	CheckBarriersForResize(deviceContext);
#endif
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb)
{
#ifndef DISABLE_BARRIERS
	ID3D12Resource* res = sb->AsD3D12Resource(deviceContext);
	if (!res )
	{
		SIMUL_CERR_ONCE << "No valid UAV resource for this barrier. No barrier was inserted into this command list.";
		return;
	}

	auto& barrier = mPendingBarriers[mCurBarriers++];
	barrier = CD3DX12_RESOURCE_BARRIER::UAV(res);
	
#if SIMUL_DEBUG_BARRIERS
	SIMUL_COUT<<"Barrier : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<std::endl;
#endif
/*	if (true)
	{
		FlushBarriers(deviceContext);
	}*/
	CheckBarriersForResize(deviceContext);
#endif
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* tex)
{
#ifndef DISABLE_BARRIERS
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();
	//immediateContext.platform_context = deviceContext.platform_context;
	dx12::Texture* t12 = (dx12::Texture*)tex;

	ID3D12Resource* res = t12->AsD3D12Resource();
	if (!res || !t12->AsD3D12UnorderedAccessView(deviceContext))
	{
		SIMUL_CERR_ONCE << "No valid UAV resource for this barrier. No barrier was inserted into this command list.";
		return;
	}

	auto& barrier = mPendingBarriers[mCurBarriers++];
	barrier = CD3DX12_RESOURCE_BARRIER::UAV(res);
	
#if SIMUL_DEBUG_BARRIERS
	SIMUL_COUT<<"Barrier : 0x"<<std::setfill('0') << std::setw(16)<<std::hex<<(unsigned long long)res<<std::endl;
#endif
	if (true)
	{
//		FlushBarriers(deviceContext);
	}
	CheckBarriersForResize(deviceContext);
#endif
}


void RenderPlatform::CheckBarriersForResize(crossplatform::DeviceContext &deviceContext)
{
	// Check if we need more space:
	if (mCurBarriers >= mTotalBarriers)
	{
		FlushBarriers(deviceContext);
		mTotalBarriers += 16;
		mPendingBarriers.resize(mTotalBarriers);
		//SIMUL_COUT << "[PERF] Resizing barrier holder to: " << mTotalBarriers << std::endl;
	}
}

void RenderPlatform::FlushBarriers(crossplatform::DeviceContext& deviceContext)
{
    if (mCurBarriers <= 0) 
    {
        return; 
    }
#if SIMUL_DEBUG_BARRIERS
	if(deviceContext.deviceContextType==barrierDeviceContextType)
	{
		SIMUL_COUT<<"\t\tFlush "<<mCurBarriers<<" barriers."<<std::endl;
	}
#endif
#ifndef DISABLE_BARRIERS
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();

#if SIMUL_DEBUG_BARRIERS
	for(size_t i = 0; i < mCurBarriers; i++)
		commandList->ResourceBarrier(1, &mPendingBarriers[i]);
#else
    commandList->ResourceBarrier(mCurBarriers, mPendingBarriers.data());
#endif

#endif
    mCurBarriers = 0;
}

void RenderPlatform::SynchronizeCacheAndState(crossplatform::DeviceContext &deviceContext) 
{
	FlushBarriers(deviceContext);
}

void RenderPlatform::PushToReleaseManager(ID3D12DeviceChild* res, const char *n)
{
    if (!res)
    {
		return;
    }
	std::string dName=n;
#if PLATFORM_D3D12_RELEASE_MANAGER_CHECKS
	char name[20];
	GetD3DName(res,name,19);
	name[19]=0;
	SIMUL_COUT<<"Push "<<name<<" (0x"<<std::hex<<""<<res<<") to release manager.\n";
	// Don't add duplicates, this operation can be potentially slow if we have tons of resources
	for (unsigned int i = 0; i < mResourceBin.size(); i++)
	{
        if (res == mResourceBin[i].second.second)  
        {
			SIMUL_CERR<<(n?n:"")<<" "<<(unsigned long long)res<<" Pushed to release manager twice."<<std::endl;
			return;
        }
	}
#endif
#if 0//PLATFORM_D3D12_RELEASE_MANAGER_CHECKS
	res->AddRef();
	int count=res->Release();
	SIMUL_COUT<<(n?n:"")<<" "<<(unsigned long long)res<<" Pushed to release manager with "<<count<<" refs remaining."<<std::endl;
#endif
	mResourceBin.push_back(std::pair<int, std::pair<std::string, ID3D12DeviceChild*>>
	(
		0,
		std::pair<std::string, ID3D12DeviceChild*>
		(
			dName,
			res
		)
	));
}

void RenderPlatform::ClearIA(crossplatform::DeviceContext &deviceContext)
{
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();
	commandList->IASetVertexBuffers(0, 1, nullptr); // Only 1? 
	commandList->IASetIndexBuffer(nullptr);
}

void RenderPlatform::RestoreDeviceObjects(void* device)
{
	isInitialized	= true;
	mCurInputLayout	= nullptr;

	if (m12Device == device && device != nullptr)
	{
		return;
	}
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device* m12Device5=nullptr;
#endif
	if (m12Device != device)
	{
		m12Device = (ID3D12Device*)device;
		renderingFeatures = crossplatform::RenderingFeatures::None;
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
		// Check feature support.
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
		
        if(S_OK==m12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		{
			bool rt=(featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
			if(rt)
				renderingFeatures=(crossplatform::RenderingFeatures)((uint32_t)renderingFeatures|(uint32_t)crossplatform::RenderingFeatures::Raytracing);
			m12Device5 = AsD3D12Device5();
		}
#endif
	}
	//immediateContext.platform_context = commandList;

    DefaultBlendState   = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    DefaultRasterState  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    DefaultDepthState   = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    DefaultOutputFormat = crossplatform::PixelFormat::RGBA_16_FLOAT;

	// Lets query some information from the device
	D3D12_FEATURE_DATA_D3D12_OPTIONS featureOptions;
	m12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureOptions, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS));
	mResourceBindingLimits.BindingTier = featureOptions.ResourceBindingTier;
	// TIER1
	if (featureOptions.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
	{
		mResourceBindingLimits.MaxShaderVisibleDescriptors	= 1000000;
		mResourceBindingLimits.MaxCBVPerStage				= 14;
		mResourceBindingLimits.MaxSRVPerStage				= 128;
		mResourceBindingLimits.MaxUAVPerStage				= 8;
		mResourceBindingLimits.MaxSamplersPerStage			= 16;
	}
	// TIER2
	else if (featureOptions.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_2)
	{
		mResourceBindingLimits.MaxShaderVisibleDescriptors	= 1000000;
		mResourceBindingLimits.MaxCBVPerStage				= 14;
		mResourceBindingLimits.MaxSRVPerStage				= 1000000;
		mResourceBindingLimits.MaxUAVPerStage				= 64;
		mResourceBindingLimits.MaxSamplersPerStage			= 1000000;
	}
	// TIER3
	else
	{
		mResourceBindingLimits.MaxShaderVisibleDescriptors	= 1000000; // +1000000;
		mResourceBindingLimits.MaxCBVPerStage				= 1000000;
		mResourceBindingLimits.MaxSRVPerStage				= 1000000;
		mResourceBindingLimits.MaxUAVPerStage				= 1000000;
		mResourceBindingLimits.MaxSamplersPerStage			= 1000000;
	}
	if(mFrameHeap)
	for(int i=0;i<3;i++)
	{
		mFrameHeap[i].Release();
	}
	if(mFrameOverrideSamplerHeap)
	for(int i=0;i<3;i++)
	{
		mFrameOverrideSamplerHeap[i].Release();
	}
	delete [] mFrameHeap;
	mFrameHeap=nullptr;
	delete [] mFrameOverrideSamplerHeap;
	mFrameOverrideSamplerHeap=nullptr;

	// Create the frame heaps
	// These heaps will be shader visible as they will be the ones bound to the command list
	mFrameHeap			        = new dx12::Heap[3];
    mFrameOverrideSamplerHeap   = new dx12::Heap[3];
	UINT maxFrameDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 / 1;
	for (unsigned int i = 0; i < 3; i++)
	{
		mFrameHeap[i].Restore(this, maxFrameDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,base::QuickFormat("FrameHeap %d",i));
        mFrameOverrideSamplerHeap[i].Restore(this, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "FrameOverrideSamplerHeap");
	}
	
	SAFE_DELETE(mSamplerHeap);
	SAFE_DELETE(mRenderTargetHeap);
	SAFE_DELETE(mDepthStencilHeap);
	SAFE_DELETE(mNullHeap);
	// Create storage heaps
	// These heaps wont be shader visible as they will be the source of the copy operation
	// we use these as storage
	mSamplerHeap = new dx12::Heap;
	mSamplerHeap->Restore(this, 64, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "SamplerHeap", false);
	mRenderTargetHeap = new dx12::Heap;
	mRenderTargetHeap->Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "RenderTargetsHeap", false);
	mDepthStencilHeap = new dx12::Heap;
	mDepthStencilHeap->Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "DepthStencilsHeap", false);
	mNullHeap = new dx12::Heap;
	mNullHeap->Restore(this, 16, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,"NullHeap",false);

	// Create dummy textures
	mDummy2D = CreateTexture("Dummy2D");
	mDummy3D = CreateTexture("Dummy3D");
	mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);
	mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);

	// Create null descriptors
	// CBV
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		mNullCBV								= mNullHeap->CpuHandle();
		m12Device->CreateConstantBufferView(&cbvDesc, mNullCBV);
		mNullHeap->Offset();
	}
	// SRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement				= 0;
		srvDesc.Buffer.NumElements				= 1;
		srvDesc.Buffer.StructureByteStride		= 1;
		srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;
		mNullSRV                                = mNullHeap->CpuHandle();
		m12Device->CreateShaderResourceView(nullptr,&srvDesc, mNullSRV);
		mNullHeap->Offset();
	}
	// UAV
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension					= D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format							= DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement				= 0;
		uavDesc.Buffer.NumElements				= 1;
		uavDesc.Buffer.StructureByteStride		= 1;
		mNullUAV                                = mNullHeap->CpuHandle();
		m12Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, mNullUAV);
		mNullHeap->Offset();
	}
	// SAMPLER
	{
		D3D12_SAMPLER_DESC sampDesc = {};
		sampDesc.AddressU			= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampDesc.AddressV			= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampDesc.AddressW			= D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampDesc.Filter				= D3D12_FILTER_MIN_MAG_MIP_POINT;
		mNullSampler                = mSamplerHeap->CpuHandle();
		m12Device->CreateSampler(&sampDesc, mNullSampler);
		mSamplerHeap->Offset();
	}

	HRESULT res						= S_FALSE;

#if defined( _XBOX_ONE) ||  defined(_GAMING_XBOX)
	// Refer to UE4:(XboxOneD3D12Device.cpp) FXboxOneD3D12DynamicRHI::GetHardwareGPUFrameTime() 
	mTimeStampFreq					= D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY;
#else
    // Time stamp freq:
	D3D12_COMMAND_QUEUE_DESC qdesc	= {};
	qdesc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	qdesc.Type						= D3D12_COMMAND_LIST_TYPE_DIRECT;
	ID3D12CommandQueue* queue		= nullptr;
	res = m12Device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));
	SIMUL_ASSERT(res == S_OK);
	res = queue->GetTimestampFrequency(&mTimeStampFreq);
	SIMUL_ASSERT(res == S_OK);
	SAFE_RELEASE(queue);
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		res = m12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m12Queue));
		SIMUL_ASSERT(res == S_OK);

		std::string str = base::QuickFormat(" mQueue " );
		m12Queue->SetName(simul::base::StringToWString(str).c_str());
	}
#endif

	// Load the RootSignature blobs - Graphics
	{
		ID3DBlob *blob=nullptr;
		ID3DBlob *error=nullptr;
		// Global Root Signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		D3D12_DESCRIPTOR_RANGE cbvSrvUavDescriptorRanges[]={{},{},{}};
		memset(cbvSrvUavDescriptorRanges,0,sizeof(cbvSrvUavDescriptorRanges));
		D3D12_DESCRIPTOR_RANGE &cbvDescriptorRange = cbvSrvUavDescriptorRanges[0];
		cbvDescriptorRange.BaseShaderRegister = 0;
		cbvDescriptorRange.NumDescriptors = 14;
		cbvDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		cbvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

		D3D12_DESCRIPTOR_RANGE &srvDescriptorRange = cbvSrvUavDescriptorRanges[1];
		srvDescriptorRange.BaseShaderRegister = 0;
		srvDescriptorRange.NumDescriptors = 24;
		srvDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		D3D12_DESCRIPTOR_RANGE &uavDescriptorRange = cbvSrvUavDescriptorRanges[2];
		uavDescriptorRange.BaseShaderRegister = 0;
		uavDescriptorRange.NumDescriptors = 16;
		uavDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		uavDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

		D3D12_DESCRIPTOR_RANGE samplerDescriptorRange = {};
		samplerDescriptorRange.BaseShaderRegister = 0;
		samplerDescriptorRange.NumDescriptors = 16;
		samplerDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		samplerDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

		CD3DX12_ROOT_PARAMETER rootParameters[2];
		memset(rootParameters,0,sizeof(rootParameters));
		rootParameters[0].InitAsDescriptorTable(3, cbvSrvUavDescriptorRanges);
		rootParameters[1].InitAsDescriptorTable(1, &samplerDescriptorRange);
		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(ARRAYSIZE(rootParameters), rootParameters);
		rsDesc.Flags|=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		HRESULT res=D3D12SerializeRootSignature(&rsDesc,D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if(res!=S_OK)
		{
			std::string err=static_cast<const char *>(error->GetBufferPointer());
			SIMUL_BREAK(err.c_str());
		}
		V_CHECK(m12Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), SIMUL_PPV_ARGS(&mGRootSignature)));
		
		mGRootSignature->SetName(L"Graphics Root Signature");
	}
	// Load the RootSignature blobs - Compute
	//mCRootSignature = mGRootSignature // Disabled as we use the Graphics one for Compute passes - AJR.
	// Load the RootSignature blobs - Raytracing Global
	{
		ID3DBlob *blob=nullptr;
		ID3DBlob *error=nullptr;
		// Global Root Signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		D3D12_DESCRIPTOR_RANGE cbvSrvUavDescriptorRanges[3];
		memset(cbvSrvUavDescriptorRanges,0,sizeof(cbvSrvUavDescriptorRanges));
		D3D12_DESCRIPTOR_RANGE &cbvDescriptorRange = cbvSrvUavDescriptorRanges[0];
		cbvDescriptorRange.BaseShaderRegister = 0;
		cbvDescriptorRange.NumDescriptors = 14;
		cbvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//cbvDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		D3D12_DESCRIPTOR_RANGE &srvDescriptorRange = cbvSrvUavDescriptorRanges[1];
		srvDescriptorRange.BaseShaderRegister = 0;
		srvDescriptorRange.NumDescriptors = 24;
		srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//srvDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		D3D12_DESCRIPTOR_RANGE &uavDescriptorRange = cbvSrvUavDescriptorRanges[2];
		uavDescriptorRange.BaseShaderRegister = 0;
		uavDescriptorRange.NumDescriptors = 16;
		uavDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//uavDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		D3D12_DESCRIPTOR_RANGE samplerDescriptorRange = {};
		samplerDescriptorRange.BaseShaderRegister = 0;
		samplerDescriptorRange.NumDescriptors = 16;
		samplerDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		samplerDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//samplerDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		
		D3D12_DESCRIPTOR_RANGE sceneBuffersDescriptorRange = {};
		sceneBuffersDescriptorRange.BaseShaderRegister =25;
		sceneBuffersDescriptorRange.NumDescriptors = 1;
		sceneBuffersDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		sceneBuffersDescriptorRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//sceneBuffersDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
		
		//CD3DX12_DESCRIPTOR_RANGE1 UAVDescriptor;
		//UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER rootParameters[3];
		memset(rootParameters,0,sizeof(rootParameters));
		rootParameters[0].InitAsDescriptorTable(3, cbvSrvUavDescriptorRanges);
		rootParameters[1].InitAsDescriptorTable(1, &samplerDescriptorRange);
		rootParameters[2].InitAsShaderResourceView(25);
		CD3DX12_ROOT_SIGNATURE_DESC rsDesc(ARRAYSIZE(rootParameters), rootParameters);
		//rsDesc.Flags=D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;;
		//rsDesc.Desc_1_1.Flags|=D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		//rsDesc.Version=D3D_ROOT_SIGNATURE_VERSION_1_1;
		HRESULT res=D3D12SerializeRootSignature(&rsDesc,D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
		if(res!=S_OK)
		{
			std::string err=static_cast<const char *>(error->GetBufferPointer());
			SIMUL_BREAK(err.c_str());
		}
		V_CHECK(m12Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), SIMUL_PPV_ARGS(&mGRaytracingGlobalSignature)));
		mGRaytracingGlobalSignature->SetName(L"Raytracing Global Root Signature");
		//mGRaytracingSignature	=LoadRootSignature("//RTX.cso");
	}
#ifndef _XBOX_ONE	
#if PLATFORM_D3D12_RELEASE_MANAGER_CHECKS
	D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings));
#endif
#endif
	crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);
	RecompileShaders();

	generalFence= CreateFence();
}

ID3D12RootSignature *RenderPlatform::LoadRootSignature(const char *filename)
{
	ID3D12RootSignature *rs=nullptr;
	auto fileLoader = base::FileLoader::GetFileLoader();
	std::vector<uint8_t> rblob;
	void* fileContents			= nullptr;
	unsigned int loadedBytes	= 0;
	fileLoader->AcquireFileContents(fileContents, loadedBytes, filename,GetShaderBinaryPathsUtf8(), false);
	if (!fileContents || loadedBytes <= 0)
	{
		SIMUL_CERR << "Could not load the RootSignature "<<filename<<".\n";
		return nullptr;
	}
	rblob.resize(loadedBytes);
	memcpy(rblob.data(), fileContents, loadedBytes);
	fileLoader->ReleaseFileContents(fileContents);
	HRESULT res = m12Device->CreateRootSignature
     (
         0, 
		rblob.data(),
         rblob.size(), 
         SIMUL_PPV_ARGS(&rs)
     );
	SIMUL_ASSERT(res == S_OK);
     if (rs)
	{
		rs->SetName(base::StringToWString(filename).c_str());
	}
     // If we call this (D3D12CreateRootSignatureDeserializer) d3d12.dll won't be delay loaded 
#if 0
        // Finally lets check which slots does the rs expect
        ID3D12RootSignatureDeserializer* rsDeserial = nullptr;
        res                                         = D3D12CreateRootSignatureDeserializer
        (
			rblob.data(), rblob.size(), SIMUL_PPV_ARGS(&rsDeserial)
        );
        SIMUL_ASSERT(res == S_OK);
        D3D12_ROOT_SIGNATURE_DESC rsDesc            = {};
        rsDesc                                      = *rsDeserial->GetRootSignatureDesc();
        if (rsDesc.NumParameters == 2)
        {
            D3D12_ROOT_PARAMETER param = rsDesc.pParameters[0];
            // CBV_SRV_UAV
            if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            {
                D3D12_ROOT_DESCRIPTOR_TABLE table = param.DescriptorTable;
                if (table.NumDescriptorRanges == 3)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        D3D12_DESCRIPTOR_RANGE range = table.pDescriptorRanges[i];
                        if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
                        {
                            SIMUL_ASSERT(ResourceBindingLimits::NumCBV == range.NumDescriptors);
                        }
                        else if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                        {
                            SIMUL_ASSERT(ResourceBindingLimits::NumUAV == range.NumDescriptors);
                        }
                        else if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
                        {
                            SIMUL_ASSERT(ResourceBindingLimits::NumSRV == range.NumDescriptors);
                        }
                        else
                        {
                            SIMUL_CERR << "Unexpected range type in root signature. \n";
                        }
                    }
                }
            }
            // SAMPLERS
            param = rsDesc.pParameters[1];
            SIMUL_ASSERT(ResourceBindingLimits::NumSamplers == param.DescriptorTable.pDescriptorRanges[0].NumDescriptors);
        }

        // Check against the hardware limits:
        if (ResourceBindingLimits::NumUAV > mResourceBindingLimits.MaxUAVPerStage)
        {
            SIMUL_CERR << "Current max num uav: " << ResourceBindingLimits::NumUAV << ", but hardware only supports: " << mResourceBindingLimits.MaxUAVPerStage << std::endl;
        }
        if (ResourceBindingLimits::NumSRV > mResourceBindingLimits.MaxSRVPerStage)
        {
            SIMUL_CERR << "Current max num srv: " << ResourceBindingLimits::NumSRV << ", but hardware only supports: " << mResourceBindingLimits.MaxSRVPerStage << std::endl;
        }
        if (ResourceBindingLimits::NumCBV > mResourceBindingLimits.MaxCBVPerStage)
        {
            SIMUL_CERR << "Current max num cbv: " << ResourceBindingLimits::NumCBV << ", but hardware only supports: " << mResourceBindingLimits.MaxCBVPerStage << std::endl;
        }
        if (ResourceBindingLimits::NumSamplers > mResourceBindingLimits.MaxSaplerPerStage)
        {
            SIMUL_CERR << "Current max num samplers: " << ResourceBindingLimits::NumSamplers << ", but hardware only supports: " << mResourceBindingLimits.NumSamplers << std::endl;
        }

		// now re-create:
		{
			ID3DBlob *blob=nullptr;
			ID3DBlob *error=nullptr;
			HRESULT res=D3D12SerializeRootSignature(&rsDesc,D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
			if(res!=S_OK)
			{
				std::string err=static_cast<const char *>(error->GetBufferPointer());
				SIMUL_BREAK(err.c_str());
			}
			V_CHECK(m12Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), SIMUL_PPV_ARGS(&rs)));
		}
        rsDeserial->Release();
		rblob.clear();
#endif
	return rs;
}
void RenderPlatform::InvalidateDeviceObjects()
{
	delete generalFence;
	generalFence=nullptr;
	if(gpuProfiler)
		gpuProfiler->InvalidateDeviceObjects();
	if(mFrameHeap)
		for(int i=0;i<3;i++)
		{
			mFrameHeap[i].Release();
		}
	if(mFrameOverrideSamplerHeap)
	for(int i=0;i<3;i++)
	{
		mFrameOverrideSamplerHeap[i].Release();
	}
	delete [] mFrameHeap;
	mFrameHeap=nullptr;
	delete [] mFrameOverrideSamplerHeap;
	mFrameOverrideSamplerHeap=nullptr;
	
	SAFE_DELETE(mSamplerHeap);
	SAFE_DELETE(mRenderTargetHeap);
	SAFE_DELETE(mDepthStencilHeap);
	SAFE_DELETE(mNullHeap);

	SAFE_DELETE(mDummy2D);
	SAFE_DELETE(mDummy3D);
	SAFE_RELEASE(mGRootSignature);
	SAFE_RELEASE(mCRootSignature);
	SAFE_RELEASE(mGRaytracingLocalSignature);
	SAFE_RELEASE(mGRaytracingGlobalSignature);
	
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	for (int i =0;i<mResourceBin.size();i++)
	{
		auto ptr = mResourceBin[i].second.second;
        if (ptr)
        {
#if PLATFORM_D3D12_RELEASE_MANAGER_CHECKS
			char name[20];
			GetD3DName(ptr,name,19);
			name[19]=0;
			SIMUL_COUT<<"Releasing: "<<name<<" (0x"<<std::hex<<ptr<<".\n";
            int remainRefs = ptr->Release();
			if(remainRefs)
			{
				SIMUL_COUT<<"Resource "<< mResourceBin[i].second.first.c_str()<<" "<<(unsigned long long)ptr<<" has "<<remainRefs<<" refs remaining."<<std::endl;
			}
			else
			{
				SIMUL_COUT<<"Resource "<< mResourceBin[i].second.first.c_str()<<" "<<(unsigned long long)ptr<<" freed."<<std::endl;
			}
#else
            ptr->Release();
#endif
			if (GetMemoryInterface()) 
				GetMemoryInterface()->UntrackVideoMemory(ptr);
        }
	}
	mResourceBin.clear();
	SAFE_RELEASE(m12Queue);
}

void RenderPlatform::RecompileShaders()
{
	if(!m12Device)
		return;
	shaders.clear();
	crossplatform::RenderPlatform::RecompileShaders();
}

void RenderPlatform::BeginEvent(crossplatform::DeviceContext &deviceContext,const char *name)
{
#ifdef SIMUL_ENABLE_PIX
	typedef HRESULT(WINAPI* PFN_PIXBeginEventOnCommandList)(ID3D12GraphicsCommandList*, UINT64, _In_ PCSTR);
	if (hWinPixEventRuntime != 0)
	{
		PFN_PIXBeginEventOnCommandList PIXBeginEventOnCommandList = (PFN_PIXBeginEventOnCommandList)GetProcAddress(hWinPixEventRuntime, "PIXBeginEventOnCommandList");
		if (PIXBeginEventOnCommandList)
		{
			PIXBeginEventOnCommandList(deviceContext.asD3D12Context(), 0, name);
		}
	}
#endif
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext &deviceContext)
{
#ifdef SIMUL_ENABLE_PIX
	typedef HRESULT(WINAPI* PFN_PIXEndEventOnCommandList)(ID3D12GraphicsCommandList*);
	if (hWinPixEventRuntime != 0)
	{
		PFN_PIXEndEventOnCommandList PIXEndEventOnCommandList = (PFN_PIXEndEventOnCommandList)GetProcAddress(hWinPixEventRuntime, "PIXEndEventOnCommandList");
		if (PIXEndEventOnCommandList)
		{
			PIXEndEventOnCommandList(deviceContext.asD3D12Context());
		}
	}
#endif
}

void RenderPlatform::BeginFrame(crossplatform::GraphicsDeviceContext &deviceContext)
{
	crossplatform::RenderPlatform::BeginFrame(deviceContext);
	BeginD3D12Frame();
}

void RenderPlatform::BeginD3D12Frame()
{
	// Store a reference to the device context
	auto &deviceContext=GetImmediateContext();
	ID3D12GraphicsCommandList*	commandList                        = deviceContext.asD3D12Context();
	//immediateContext.platform_context   = deviceContext.platform_context;

	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix(GetImmediateContext().viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_TEST_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_TEST_DEPTH_LESS_EQUAL);

	if(!bImmediateContextActive&&!bExternalImmediate)
		commandList->Reset(mImmediateAllocator, nullptr);
	bImmediateContextActive=true;
	// Create dummy textures
	static bool createDummy = true;
	if (createDummy)
	{
		const uint_fast8_t dummyData[4] = { 1,1,1,1 };
		mDummy2D->setTexels(deviceContext, &dummyData[0], 0, 1);
		mDummy3D->setTexels(deviceContext, &dummyData[0], 0, 1);
		createDummy = false;
	}

	// Age and delete old objects
	unsigned int kMaxAge = 8;
	if (!mResourceBin.empty())
	{
		for (int i = (int)(mResourceBin.size() - 1); i >= 0; i--)
		{
			mResourceBin[i].first++;
			if ((unsigned int)mResourceBin[i].first >= kMaxAge)
			{
				int remainRefs = 0;
				ID3D12DeviceChild* ptr = mResourceBin[i].second.second;
                if (ptr)
                {
                    remainRefs = ptr->Release();
                }
#if PLATFORM_D3D12_RELEASE_MANAGER_CHECKS
				if (remainRefs)
					SIMUL_CERR << mResourceBin[i].second.first << " is still referenced( " << remainRefs << " " << std::endl;
#endif
				if (GetMemoryInterface())
					GetMemoryInterface()->UntrackVideoMemory(ptr); 
				mResourceBin.erase(mResourceBin.begin() + i);
			}
		}
	}
}

void RenderPlatform::EndFrame(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::EndFrame(deviceContext);
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	if(commandList&&bImmediateContextActive&&!bExternalImmediate)
		commandList->Close();
	bImmediateContextActive=false;
}

void RenderPlatform::ResourceTransition(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* t, crossplatform::ResourceTransition transition)
{
    //commandList                        = deviceContext.asD3D12Context();
   //immediateContext.platform_context   = deviceContext.platform_context;
    dx12::Texture* t12                  = (dx12::Texture*)t;

    switch (transition)
    {
        case simul::crossplatform::Readable: 
        {
            t12->AsD3D12ShaderResourceView(deviceContext,true, crossplatform::ShaderResourceType::TEXTURE_2D); 
            break;
        }
        case simul::crossplatform::Writeable:
        {
            if (t12->HasRenderTargets())
            {
                t12->AsD3D12RenderTargetView(deviceContext);
            }
            else if (t12->IsDepthStencil())
            {
                t12->AsD3D12DepthStencilView(deviceContext);
            }
            break;
        }
        case simul::crossplatform::UnorderedAccess:
        {
            t12->AsD3D12UnorderedAccessView(deviceContext);
            break;
        }
    }

   // FlushBarriers(deviceContext);
}

void RenderPlatform::CopyTexture(crossplatform::DeviceContext& deviceContext,crossplatform::Texture *t,crossplatform::Texture *s)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context	= deviceContext.platform_context;

	auto src			= (dx12::Texture*)s;
	auto dst			= (dx12::Texture*)t;
	if (!src || !dst)
	{
		SIMUL_CERR << "Passed a null texture to CopyTexture(), ignoring call.\n";
		return;
	}

	// Ensure textures are compatible
	auto srcDesc = src->AsD3D12Resource()->GetDesc();
	auto dstDesc = dst->AsD3D12Resource()->GetDesc();
	if(	(srcDesc.Width != dstDesc.Width)						||
		(srcDesc.Height != dstDesc.Height)						||
		(srcDesc.DepthOrArraySize != dstDesc.DepthOrArraySize)	||
		(srcDesc.MipLevels != dstDesc.MipLevels))
	{
		SIMUL_CERR << "Passed incompatible textures to CopyTexture(), both textures should have same width,height,depth and mip level, ignoring call.\n";
		SIMUL_CERR << "Src: width: " << srcDesc.Width << ", height: " << srcDesc.Height << ", depth: " << srcDesc.DepthOrArraySize << ", mips: " << srcDesc.MipLevels << std::endl;
		SIMUL_CERR << "Dst: width: " << dstDesc.Width << ", height: " << dstDesc.Height << ", depth: " << dstDesc.DepthOrArraySize << ", mips: " << dstDesc.MipLevels << std::endl;
		return;
	}
	
	// Ensure source state
	bool changedSrc = false;
	auto srcState	= src->GetCurrentState(deviceContext);
	if ((srcState & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changedSrc = true;
		ResourceTransitionSimple(deviceContext,src->AsD3D12Resource(), srcState, D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	}
	
	// Ensure dst state
	bool changedDst = false;
	auto dstState	= dst->GetCurrentState(deviceContext);
	if ((dstState & D3D12_RESOURCE_STATE_COPY_DEST) != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		changedDst = true;
		ResourceTransitionSimple(deviceContext,dst->AsD3D12Resource(), dstState, D3D12_RESOURCE_STATE_COPY_DEST,true);
	}
	
	// Perform the copy. This is done GPU side and does not incur much CPU overhead (if copying full resources)
	commandList->CopyResource(dst->AsD3D12Resource(), src->AsD3D12Resource());
	
	// Reset to the original states
	if (changedSrc)
	{
		ResourceTransitionSimple(deviceContext,src->AsD3D12Resource(), D3D12_RESOURCE_STATE_COPY_SOURCE, srcState,true);
	}
	if (changedDst)
	{
		ResourceTransitionSimple(deviceContext,dst->AsD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, dstState,true);
	}
}

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();

	ApplyContextState(deviceContext);
	commandList->Dispatch(w, l, d);
}

void RenderPlatform::DispatchRays(crossplatform::DeviceContext &deviceContext, const uint3 &dispatch, const crossplatform::ShaderBindingTable* sbt)
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device5* m12Device5 = AsD3D12Device5();
	if(!m12Device5||!HasRenderingFeatures(crossplatform::RenderingFeatures::Raytracing))
		return;

	ApplyContextState(deviceContext);
	ID3D12GraphicsCommandList5*	commandList =static_cast<ID3D12GraphicsCommandList5*>(deviceContext.asD3D12Context());

	const dx12::ShaderBindingTable* raytraceTable = nullptr;
	if (sbt)
	{
		raytraceTable = reinterpret_cast<const dx12::ShaderBindingTable*>(sbt);
	}
	else
	{
		dx12::EffectPass* effectPass12 = reinterpret_cast<dx12::EffectPass*>(deviceContext.contextState.currentEffectPass);
		raytraceTable = reinterpret_cast<const dx12::ShaderBindingTable*>(effectPass12->GetShaderBindingTable()); //C++ weirdness here, we need to use reinterpret_cast<> for type conversion safety.
	}
	if (!raytraceTable)
		return;

    D3D12_DISPATCH_RAYS_DESC d3d12DispatchDesc = {};
	if (raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::RAYGEN))
	{
		d3d12DispatchDesc.RayGenerationShaderRecord.StartAddress	= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::RAYGEN)->GetGPUVirtualAddress();
		d3d12DispatchDesc.RayGenerationShaderRecord.SizeInBytes		= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::RAYGEN)->GetDesc().Width;
	}
	if (raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::MISS))
	{
		d3d12DispatchDesc.MissShaderTable.StartAddress				= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::MISS)->GetGPUVirtualAddress();
		d3d12DispatchDesc.MissShaderTable.SizeInBytes				= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::MISS)->GetDesc().Width;
		d3d12DispatchDesc.MissShaderTable.StrideInBytes				= raytraceTable->GetShaderBindingTableStrides().at(crossplatform::ShaderRecord::Type::MISS);
	}
	if (raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::HIT_GROUP))
	{
		d3d12DispatchDesc.HitGroupTable.StartAddress				= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::HIT_GROUP)->GetGPUVirtualAddress();
		d3d12DispatchDesc.HitGroupTable.SizeInBytes					= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::HIT_GROUP)->GetDesc().Width;
		d3d12DispatchDesc.HitGroupTable.StrideInBytes				= raytraceTable->GetShaderBindingTableStrides().at(crossplatform::ShaderRecord::Type::HIT_GROUP);
	}
	if (raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::CALLABLE))
	{
		d3d12DispatchDesc.CallableShaderTable.StartAddress			= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::CALLABLE)->GetGPUVirtualAddress();
		d3d12DispatchDesc.CallableShaderTable.SizeInBytes			= raytraceTable->SBTResources.at(crossplatform::ShaderRecord::Type::CALLABLE)->GetDesc().Width;
		d3d12DispatchDesc.CallableShaderTable.StrideInBytes			= raytraceTable->GetShaderBindingTableStrides().at(crossplatform::ShaderRecord::Type::CALLABLE);
	}
    d3d12DispatchDesc.Width											= dispatch.x;
    d3d12DispatchDesc.Height										= dispatch.y;
    d3d12DispatchDesc.Depth											= dispatch.z;
	commandList->DispatchRays(&d3d12DispatchDesc);
#endif
}

void RenderPlatform::Signal(crossplatform::DeviceContext& deviceContext, Fence* fence, unsigned long long value)
{
	dx12::Fence *f=fence;
	m12Queue->Signal(f->AsD3d12Fence(),value);
}

void RenderPlatform::Draw(crossplatform::GraphicsDeviceContext &deviceContext,int num_verts,int start_vert)
{
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();


	ApplyContextState(deviceContext);
	commandList->DrawInstanced(num_verts,1,start_vert,0);
}

void RenderPlatform::DrawIndexed(crossplatform::GraphicsDeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();


	ApplyContextState(deviceContext);
	commandList->DrawIndexedInstanced(num_indices, 1, start_index, base_vert, 0);
}

void RenderPlatform::ApplyDefaultMaterial()
{
}

crossplatform::Texture *RenderPlatform::createTexture()
{
	crossplatform::Texture * tex=NULL;
	tex=new dx12::Texture();
	return tex;
}

crossplatform::BaseFramebuffer *RenderPlatform::CreateFramebuffer(const char *name)
{
	return new dx12::Framebuffer(name);
}

static D3D12_TEXTURE_ADDRESS_MODE ToD3D12TextureAddressMode(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch (w)	
	{
	case simul::crossplatform::SamplerStateDesc::WRAP:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case simul::crossplatform::SamplerStateDesc::CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case simul::crossplatform::SamplerStateDesc::MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}
static D3D12_FILTER ToD3D12Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	switch (f)
	{
	case simul::crossplatform::SamplerStateDesc::POINT:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	case simul::crossplatform::SamplerStateDesc::LINEAR:
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	case simul::crossplatform::SamplerStateDesc::ANISOTROPIC:
		return D3D12_FILTER_ANISOTROPIC;
	default: 
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	}
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	const float border[4] = { 0.0f,0.0f,0.0f,0.0f };
	dx12::SamplerState *s = new dx12::SamplerState(d);

	D3D12_SAMPLER_DESC s12	= {};
    s12.Filter				= ToD3D12Filter(d->filtering);
	s12.AddressU			= ToD3D12TextureAddressMode(d->x);
	s12.AddressV			= ToD3D12TextureAddressMode(d->y);
	s12.AddressW			= ToD3D12TextureAddressMode(d->z);
	s12.ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	s12.MaxAnisotropy		= 16;
	s12.MinLOD				= 0.0f;
	s12.MaxLOD				= D3D12_FLOAT32_MAX;
	s12.MipLODBias			= 0.0f;
	
	m12Device->CreateSampler(&s12, mSamplerHeap->CpuHandle());
	s->SetDescriptorHandle( mSamplerHeap->CpuHandle());
	mSamplerHeap->Offset();

	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect()
{
	crossplatform::Effect *e= new dx12::Effect();
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx12::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx12::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx12::Buffer();
	return b;
}

DXGI_FORMAT simul::dx12::RenderPlatform::ToDxgiFormat(crossplatform::PixelOutputFormat p)
{
    switch (p)
    {
    case simul::crossplatform::FMT_UNKNOWN:
        return DXGI_FORMAT_UNKNOWN;
    case simul::crossplatform::FMT_32_GR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_32_AR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_FP16_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_UNORM16_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_SNORM16_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_UINT16_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_SINT16_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_16_FLOAT);
    case simul::crossplatform::FMT_32_ABGR:
        return ToDxgiFormat(crossplatform::RGBA_32_FLOAT);
    case simul::crossplatform::OUTPUT_FORMAT_COUNT:
    default:
        return DXGI_FORMAT_UNKNOWN;
        break;
    }
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGB_11_11_10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case RGB10_A2_UNORM:
		return DXGI_FORMAT_R10G10B10A2_UNORM; 
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RG_16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case RG_32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case INT_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case BGRA_8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case RGBA_8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case RGBA_8_UNORM_COMPRESSED:
		return DXGI_FORMAT_BC7_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case RGBA_8_UINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;
	case R_32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RG_32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case D_32_FLOAT_S_8_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case D_16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case D_24_UNORM_S_8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	};
}

crossplatform::PixelFormat RenderPlatform::FromDxgiFormat(DXGI_FORMAT f)
{
	using namespace crossplatform;
	switch(f)
	{
	case DXGI_FORMAT_R16_FLOAT:
		return R_16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return RGBA_16_FLOAT;
	case DXGI_FORMAT_R11G11B10_FLOAT:
		return RGB_11_11_10_FLOAT;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		return RGB10_A2_UNORM;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return RGBA_32_FLOAT;
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return RGB_32_FLOAT;
	case DXGI_FORMAT_R32G32_FLOAT:
		return RG_32_FLOAT;
	case DXGI_FORMAT_R32_FLOAT:
		return R_32_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return RGBA_8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return RGBA_8_UNORM_SRGB;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		// What possible reason is there for this to exist?
		return BGRA_8_UNORM;
	case DXGI_FORMAT_R32_UINT:
		return R_32_UINT;
	case DXGI_FORMAT_R32G32_UINT:
		return RG_32_UINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return RGB_32_UINT;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return RGBA_32_UINT;
	case DXGI_FORMAT_D32_FLOAT:
		return D_32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return D_32_FLOAT_S_8_UINT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return D_24_UNORM_S_8_UINT;
	case DXGI_FORMAT_D16_UNORM:
		return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

crossplatform::ShaderResourceType RenderPlatform::FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t)
{
	using namespace crossplatform;
	switch(t)
	{
	case D3D_SVT_TEXTURE1D:
		return ShaderResourceType::TEXTURE_1D;
	case D3D_SVT_TEXTURE2D:
		return ShaderResourceType::TEXTURE_2D;
	case D3D_SVT_TEXTURE3D:
		return ShaderResourceType::TEXTURE_3D;
	case D3D_SVT_TEXTURECUBE:
		return ShaderResourceType::TEXTURE_CUBE;
	case D3D_SVT_SAMPLER:
		return ShaderResourceType::SAMPLER;
	case D3D_SVT_BUFFER:
		return ShaderResourceType::BUFFER;
	case D3D_SVT_CBUFFER:
		return ShaderResourceType::CBUFFER;
	case D3D_SVT_TBUFFER:
		return ShaderResourceType::TBUFFER;
	case D3D_SVT_TEXTURE1DARRAY:
		return ShaderResourceType::TEXTURE_1D_ARRAY;
	case D3D_SVT_TEXTURE2DARRAY:
		return ShaderResourceType::TEXTURE_2D_ARRAY;
	case D3D_SVT_TEXTURE2DMS:
		return ShaderResourceType::TEXTURE_2DMS;
	case D3D_SVT_TEXTURE2DMSARRAY:
		return ShaderResourceType::TEXTURE_2DMS_ARRAY;
	case D3D_SVT_TEXTURECUBEARRAY:
		return ShaderResourceType::TEXTURE_CUBE_ARRAY;
	case D3D_SVT_RWTEXTURE1D:
		return ShaderResourceType::RW_TEXTURE_1D;
	case D3D_SVT_RWTEXTURE1DARRAY:
		return ShaderResourceType::RW_TEXTURE_1D_ARRAY;
	case D3D_SVT_RWTEXTURE2D:
		return ShaderResourceType::RW_TEXTURE_2D;
	case D3D_SVT_RWTEXTURE2DARRAY:
		return ShaderResourceType::RW_TEXTURE_2D_ARRAY;
	case D3D_SVT_RWTEXTURE3D:
		return ShaderResourceType::RW_TEXTURE_3D;
	case D3D_SVT_RWBUFFER:
		return ShaderResourceType::RW_BUFFER;
	case D3D_SVT_BYTEADDRESS_BUFFER:
		return ShaderResourceType::BYTE_ADDRESS_BUFFER;
	case D3D_SVT_RWBYTEADDRESS_BUFFER:
		return ShaderResourceType::RW_BYTE_ADDRESS_BUFFER;
	case D3D_SVT_STRUCTURED_BUFFER:
		return ShaderResourceType::STRUCTURED_BUFFER;
	case D3D_SVT_RWSTRUCTURED_BUFFER:
		return ShaderResourceType::RW_STRUCTURED_BUFFER;
	case D3D_SVT_APPEND_STRUCTURED_BUFFER:
		return ShaderResourceType::APPEND_STRUCTURED_BUFFER;
	case D3D_SVT_CONSUME_STRUCTURED_BUFFER:
		return ShaderResourceType::CONSUME_STRUCTURED_BUFFER;
	default:
		return ShaderResourceType::COUNT;
	}
}

int RenderPlatform::ByteSizeOfFormatElement(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 16;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 12;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return 8;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return 4;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return 2;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return 1;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		return 16;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_R1_UNORM:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 8;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return 4;

		// These are compressed, but bit-size information is unclear.
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
		return 4;

	case DXGI_FORMAT_UNKNOWN:
	default:
		return 0;
	}
}

bool simul::dx12::RenderPlatform::IsTypeless(DXGI_FORMAT fmt, bool partialTypeless)
{
	switch (static_cast<int>(fmt))
	{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC7_TYPELESS:
			return true;

		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return partialTypeless;

		case 119 /* DXGI_FORMAT_R16_UNORM_X8_TYPELESS */:
		case 120 /* DXGI_FORMAT_X16_TYPELESS_G8_UINT */:
			// These are Xbox One platform specific types
			return partialTypeless;

		default:
			return false;
	}
}

DXGI_FORMAT simul::dx12::RenderPlatform::DsvToTypelessFormat(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	default:break;
	};
	return fmt;
}

DXGI_FORMAT simul::dx12::RenderPlatform::TypelessToDsvFormat(DXGI_FORMAT fmt)
{
	if (!IsTypeless(fmt, true))
		return fmt;
	int u = fmt + 1;
	switch (fmt)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
	default:break;
	};
	return (DXGI_FORMAT)u;
}

DXGI_FORMAT simul::dx12::RenderPlatform::TypelessToSrvFormat(DXGI_FORMAT fmt)
{
	if (!IsTypeless(fmt, true))
		return fmt;
	int u = fmt + 1;
	switch (fmt)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:break;
	};
	return (DXGI_FORMAT)u;
}

D3D12_QUERY_TYPE simul::dx12::RenderPlatform::ToD3dQueryType(crossplatform::QueryType t)
{
	switch (t)
	{
	case crossplatform::QUERY_OCCLUSION:
		return D3D12_QUERY_TYPE_OCCLUSION;
	case crossplatform::QUERY_TIMESTAMP:
		return D3D12_QUERY_TYPE_TIMESTAMP;
	case crossplatform::QUERY_TIMESTAMP_DISJOINT:
		return D3D12_QUERY_TYPE_TIMESTAMP;
	default:
		return  D3D12_QUERY_TYPE_OCCLUSION;
	};
}

D3D12_QUERY_HEAP_TYPE simul::dx12::RenderPlatform::ToD3D12QueryHeapType(crossplatform::QueryType t)
{
	switch (t)
	{
	case simul::crossplatform::QUERY_UNKNOWN:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case simul::crossplatform::QUERY_OCCLUSION:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case simul::crossplatform::QUERY_TIMESTAMP:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	case simul::crossplatform::QUERY_TIMESTAMP_DISJOINT:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;	
	default:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	}
}

void RenderPlatform::SetCurrentSamples(int samples, int quality/*=0*/)
{
    mMsaaInfo.Count     = samples;
    mMsaaInfo.Quality   = quality;
}

bool RenderPlatform::IsMSAAEnabled()
{
    return mMsaaInfo.Count != 1;
}

DXGI_SAMPLE_DESC RenderPlatform::GetMSAAInfo()
{
	return mMsaaInfo;
}

ResourceBindingLimits RenderPlatform::GetResourceBindingLimits() const
{
	return mResourceBindingLimits;
}

ID3D12RootSignature* RenderPlatform::GetGraphicsRootSignature() const
{
	return mGRootSignature;
}

ID3D12RootSignature* RenderPlatform::GetComputeRootSignature() const
{
	return mCRootSignature;
}

ID3D12RootSignature* RenderPlatform::GetRaytracingLocalRootSignature() const
{
	return mGRaytracingLocalSignature;
}

ID3D12RootSignature* RenderPlatform::GetRaytracingGlobalRootSignature() const
{
	return mGRaytracingGlobalSignature;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderPlatform::GetNullCBV() const
{
	return mNullCBV;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderPlatform::GetNullSRV() const
{
	return mNullSRV;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderPlatform::GetNullUAV() const
{
	return mNullUAV;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderPlatform::GetNullSampler() const
{
	return mNullSampler;
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc,bool interleaved)
{
	dx12::Layout *l = new dx12::Layout();

	// Init and fill the input element desc array
	l->Dx12InputLayout.resize(num_elements);

	for (int i = 0; i < num_elements; i++)
	{
		if (strcmp(desc[i].semanticName, "POSITION") != 0)
		{
			//strcat(desc[i].semanticName, std::to_string(desc[i].semanticIndex).c_str());
		}
		l->Dx12InputLayout[i].SemanticName			= desc[i].semanticName;
		l->Dx12InputLayout[i].SemanticIndex			= desc[i].semanticIndex;
		l->Dx12InputLayout[i].Format				= ToDxgiFormat(desc[i].format);
		l->Dx12InputLayout[i].InputSlot				= desc[i].inputSlot;
		l->Dx12InputLayout[i].AlignedByteOffset		= desc[i].alignedByteOffset;
		l->Dx12InputLayout[i].InputSlotClass		= desc[i].perInstance ?	D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA :D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		l->Dx12InputLayout[i].InstanceDataStepRate	= desc[i].instanceDataStepRate;
	}

	// Fill the input layout description
	l->Dx12LayoutDesc.pInputElementDescs	= l->Dx12InputLayout.data();
	l->Dx12LayoutDesc.NumElements			= num_elements;

	l->SetDesc(desc, num_elements);

	return l;
}

static D3D12_COMPARISON_FUNC toD3d12Comparison(crossplatform::DepthComparison d)
{
	switch (d)
	{
	case simul::crossplatform::DEPTH_ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	case simul::crossplatform::DEPTH_NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case simul::crossplatform::DEPTH_LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case simul::crossplatform::DEPTH_EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case simul::crossplatform::DEPTH_LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case simul::crossplatform::DEPTH_GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case simul::crossplatform::DEPTH_NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case simul::crossplatform::DEPTH_GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	default:
		SIMUL_BREAK("Undefined comparison func");
		return D3D12_COMPARISON_FUNC_NEVER;
	}
}

static D3D12_BLEND_OP toD3d12BlendOp(crossplatform::BlendOperation o)
{
	switch (o)
	{
	case simul::crossplatform::BLEND_OP_NONE:
		return D3D12_BLEND_OP_ADD; // Just pass a default thing?
	case simul::crossplatform::BLEND_OP_ADD:
		return D3D12_BLEND_OP_ADD;
	case simul::crossplatform::BLEND_OP_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case simul::crossplatform::BLEND_OP_MAX:
		return D3D12_BLEND_OP_MAX;
	case simul::crossplatform::BLEND_OP_MIN:
		return D3D12_BLEND_OP_MIN;
	default:
		SIMUL_BREAK("Undefined blend operation");
		return D3D12_BLEND_OP_ADD;;
	}
}

static D3D12_BLEND toD3d12Blend(crossplatform::BlendOption o)
{
	switch (o)
	{
	case simul::crossplatform::BLEND_ZERO:
		return D3D12_BLEND_ZERO;
	case simul::crossplatform::BLEND_ONE:
		return D3D12_BLEND_ONE;
	case simul::crossplatform::BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case simul::crossplatform::BLEND_INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case simul::crossplatform::BLEND_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case simul::crossplatform::BLEND_INV_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case simul::crossplatform::BLEND_DEST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case simul::crossplatform::BLEND_INV_DEST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case simul::crossplatform::BLEND_DEST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case simul::crossplatform::BLEND_INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case simul::crossplatform::BLEND_SRC_ALPHA_SAT:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case simul::crossplatform::BLEND_BLEND_FACTOR:
		return D3D12_BLEND_BLEND_FACTOR;
	case simul::crossplatform::BLEND_INV_BLEND_FACTOR:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case simul::crossplatform::BLEND_SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case simul::crossplatform::BLEND_INV_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case simul::crossplatform::BLEND_SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case simul::crossplatform::BLEND_INV_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

D3D12_FILL_MODE toD3d12FillMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return D3D12_FILL_MODE_SOLID;
	case crossplatform::POLYGON_MODE_LINE:
		return D3D12_FILL_MODE_WIREFRAME;
	case crossplatform::POLYGON_MODE_POINT:
		SIMUL_BREAK("Directx12 doesn't have a POINT polygon mode");
		return D3D12_FILL_MODE_SOLID;
	default:
		SIMUL_BREAK("Undefined polygon mode");
		return D3D12_FILL_MODE_SOLID;
	}
}

D3D12_CULL_MODE toD3d12CullMode(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case simul::crossplatform::CULL_FACE_NONE:
		return D3D12_CULL_MODE_NONE;
	case simul::crossplatform::CULL_FACE_FRONT:
		return D3D12_CULL_MODE_FRONT;
	case simul::crossplatform::CULL_FACE_BACK:
		return D3D12_CULL_MODE_BACK;
	case simul::crossplatform::CULL_FACE_FRONTANDBACK:
		SIMUL_BREAK("In Directx12 there is no FRONTANDBACK cull face mode");
	default:
		SIMUL_BREAK("Undefined cull face mode");
		return D3D12_CULL_MODE_NONE;
	}
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx12::RenderState* s= new dx12::RenderState();
	s->type				= desc.type;

	if (desc.type == crossplatform::BLEND)
	{
		s->BlendDesc						= D3D12_BLEND_DESC();
		s->BlendDesc.AlphaToCoverageEnable	= desc.blend.AlphaToCoverageEnable;
		s->BlendDesc.IndependentBlendEnable = desc.blend.IndependentBlendEnable;
		for (int i = 0; i < desc.blend.numRTs; i++)	// TO-DO: Check for max simultaneos rt value?
		{
			s->BlendDesc.RenderTarget[i].BlendEnable			=	(desc.blend.RenderTarget[i].blendOperation		!= crossplatform::BLEND_OP_NONE) || 
																	(desc.blend.RenderTarget[i].blendOperationAlpha != crossplatform::BLEND_OP_NONE);
			s->BlendDesc.RenderTarget[i].BlendOp				= toD3d12BlendOp(desc.blend.RenderTarget[i].blendOperation);
			s->BlendDesc.RenderTarget[i].SrcBlend				= toD3d12Blend(desc.blend.RenderTarget[i].SrcBlend);
			s->BlendDesc.RenderTarget[i].DestBlend				= toD3d12Blend(desc.blend.RenderTarget[i].DestBlend);

			s->BlendDesc.RenderTarget[i].BlendOpAlpha			= toD3d12BlendOp(desc.blend.RenderTarget[i].blendOperationAlpha);
			s->BlendDesc.RenderTarget[i].SrcBlendAlpha			= toD3d12Blend(desc.blend.RenderTarget[i].SrcBlendAlpha);
			s->BlendDesc.RenderTarget[i].DestBlendAlpha			= toD3d12Blend(desc.blend.RenderTarget[i].DestBlendAlpha);

			s->BlendDesc.RenderTarget[i].RenderTargetWriteMask	= desc.blend.RenderTarget[i].RenderTargetWriteMask;
			s->BlendDesc.RenderTarget[i].LogicOpEnable			= false;
			s->BlendDesc.RenderTarget[i].LogicOp				= D3D12_LOGIC_OP_NOOP;
		}
	}
	else if (desc.type == crossplatform::RASTERIZER)
	{
		s->RasterDesc							= D3D12_RASTERIZER_DESC();
		s->RasterDesc.FillMode					= toD3d12FillMode(desc.rasterizer.polygonMode);
		s->RasterDesc.CullMode					= toD3d12CullMode(desc.rasterizer.cullFaceMode);
		s->RasterDesc.FrontCounterClockwise		= desc.rasterizer.frontFace != crossplatform::FrontFace::FRONTFACE_CLOCKWISE;
		s->RasterDesc.DepthBias					= 0;
		s->RasterDesc.DepthBiasClamp			= 0.0f;
		s->RasterDesc.SlopeScaledDepthBias		= 0.0f;
		s->RasterDesc.DepthClipEnable			= false;
		s->RasterDesc.MultisampleEnable			= false;	// TO-DO : what if not!!!
		s->RasterDesc.AntialiasedLineEnable		= false;
		s->RasterDesc.ForcedSampleCount			= 0;
		s->RasterDesc.ConservativeRaster		= D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}
	else if (desc.type == crossplatform::DEPTH)
	{
		s->DepthStencilDesc						= D3D12_DEPTH_STENCIL_DESC();
		s->DepthStencilDesc.DepthEnable			= desc.depth.test;
		s->DepthStencilDesc.DepthWriteMask		= desc.depth.write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		s->DepthStencilDesc.DepthFunc			= toD3d12Comparison(desc.depth.comparison);
		s->DepthStencilDesc.StencilEnable		= false;
	}
    else if (desc.type == crossplatform::RTFORMAT)
    {
        s->RtFormatDesc = {};
        int cnt = 0;
        for (int i = 0; i < 8; i++)
        {
            if (desc.rtFormat.formats[i] != crossplatform::UNKNOWN)
            {
                cnt++;
                s->RtFormatDesc.RTFormats[i] = ToDxgiFormat(desc.rtFormat.formats[i]);
            }
            else
            {
                s->RtFormatDesc.RTFormats[i] = DXGI_FORMAT_UNKNOWN;
            }
        }
        s->RtFormatDesc.Count = cnt;
    }
	else
	{
		SIMUL_BREAK("Not recognised render state type");
	}
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	dx12::Query* q = new dx12::Query(type);
	return q;
}

void Fence::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{			
	dx12::RenderPlatform *dx12R=(dx12::RenderPlatform *)r;
	V_CHECK(dx12R->AsD3D12Device()->CreateFence (0, D3D12_FENCE_FLAG_SHARED, SIMUL_PPV_ARGS (&d3d12Fence)));
}

void Fence::InvalidateDeviceObjects()
{
	SAFE_RELEASE(d3d12Fence);
}

crossplatform::Fence *RenderPlatform::CreateFence( )
{
	dx12::Fence* q = new dx12::Fence();
	return q;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	,const crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context=deviceContext.platform_context;

	if (buffers == nullptr)
		return;

	if (num_buffers > 1)
	{
		SIMUL_BREAK("Nacho has to work to do here!!");
	}
	auto pBuffer = (dx12::Buffer*)buffers[0];
	commandList->IASetVertexBuffers
	(
		0,
		num_buffers,
		pBuffer->GetVertexBufferView()
	);
};

void RenderPlatform::ClearVertexBuffers(crossplatform::DeviceContext& deviceContext)
{

}

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &,crossplatform::Buffer *,int )
{

}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext,int num,crossplatform::Texture** targs,crossplatform::Texture* depth)
{
    mTargets        = {};
    mTargets.num    = num;
    for (int i = 0; i < num; i++)
    {
        mTargets.m_rt[i]        = targs[i]->AsD3D12RenderTargetView(deviceContext,0, 0);
        mTargets.rtFormats[i]   = targs[i]->pixelFormat;
    }
    if (depth)
    {
        mTargets.m_dt           = depth->AsD3D12DepthStencilView(deviceContext);
        mTargets.depthFormat    = depth->pixelFormat;
    }
    mTargets.viewport           = { 0,0,targs[0]->GetWidth(),targs[0]->GetLength() };

    ActivateRenderTargets(deviceContext, &mTargets);
}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext,crossplatform::TargetsAndViewport* targets)
{
	// We have to flush because otherwise the barrier will occur after the switch.
	FlushBarriers(deviceContext);
    SIMUL_ASSERT(targets->num <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
    //immediateContext.platform_context                                               = deviceContext.platform_context;
    D3D12_CPU_DESCRIPTOR_HANDLE tHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]   = {};
    for (int i = 0; i < targets->num; i++)
    {
        SIMUL_ASSERT(targets->m_rt[i] != nullptr);
        tHandles[i] = *(D3D12_CPU_DESCRIPTOR_HANDLE*)targets->m_rt[i];
    }
    commandList->OMSetRenderTargets((UINT)targets->num, tHandles, FALSE, (D3D12_CPU_DESCRIPTOR_HANDLE*)targets->m_dt);
    deviceContext.targetStack.push(targets);

    SetViewports(deviceContext, 1, &targets->viewport);
}

void RenderPlatform::ApplyDefaultRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext)
{
	// We have to flush because otherwise the barrier will occur after the switch.
	FlushBarriers(deviceContext);
	if(deviceContext.defaultTargetsAndViewport.num)
	{
		for(int i=0;i<deviceContext.defaultTargetsAndViewport.num;i++)
		{
			dx12::Texture *t=(dx12::Texture*)deviceContext.defaultTargetsAndViewport.textureTargets[i].texture;
			if(t)
				t->SetLayout(deviceContext,D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}
	D3D12_CPU_DESCRIPTOR_HANDLE h[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	D3D12_CPU_DESCRIPTOR_HANDLE *D=nullptr;
	if(deviceContext.defaultTargetsAndViewport.m_rt[0]||deviceContext.defaultTargetsAndViewport.m_dt)
	{
		for (int i = 0; i < deviceContext.defaultTargetsAndViewport.num; i++)
		{
			h[i] = deviceContext.defaultTargetsAndViewport.m_rt[i]?*((CD3DX12_CPU_DESCRIPTOR_HANDLE*)deviceContext.defaultTargetsAndViewport.m_rt[i]):CD3DX12_CPU_DESCRIPTOR_HANDLE();
		}
	}
	else
	{
		for (int i = 0; i < deviceContext.defaultTargetsAndViewport.num; i++)
		{
			auto &t=deviceContext.defaultTargetsAndViewport.textureTargets[i];
			if(t.texture)
				h[i] = *(t.texture->AsD3D12RenderTargetView(deviceContext,t.layer,t.mip));
		}
	}
	auto &d=deviceContext.defaultTargetsAndViewport.depthTarget;
	if(d.texture)
		D=d.texture->AsD3D12DepthStencilView(deviceContext);
	else
		D=(D3D12_CPU_DESCRIPTOR_HANDLE*)deviceContext.defaultTargetsAndViewport.m_dt;
	
	deviceContext.asD3D12Context()->OMSetRenderTargets((UINT)deviceContext.defaultTargetsAndViewport.num, h, false, D);
	
	if(deviceContext.defaultTargetsAndViewport.viewport.w*deviceContext.defaultTargetsAndViewport.viewport.h)
	    SetViewports(deviceContext, 1, &deviceContext.defaultTargetsAndViewport.viewport);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext)
{
	// We have to flush because otherwise the barrier will occur after the switch.
	FlushBarriers(deviceContext);
    deviceContext.GetFrameBufferStack().pop();
    // Stack is empty so apply default targets:
    if (deviceContext.GetFrameBufferStack().empty())
    {
		ApplyDefaultRenderTargets(deviceContext);
    }
    // Apply top target:
    else
    {
        auto curTargets = deviceContext.GetFrameBufferStack().top();
		CD3DX12_CPU_DESCRIPTOR_HANDLE h[8];
		for (int i = 0; i < curTargets->num; i++)
		{
			h[i] = *((CD3DX12_CPU_DESCRIPTOR_HANDLE*)curTargets->m_rt[i]);
		}
        deviceContext.asD3D12Context()->OMSetRenderTargets
        (
            (UINT)curTargets->num,
            h,
            false,
            (CD3DX12_CPU_DESCRIPTOR_HANDLE*)curTargets->m_dt
        );
        SetViewports(deviceContext, 1, &curTargets->viewport);
    }
}

void RenderPlatform::SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context=deviceContext.platform_context;

	SIMUL_ASSERT(num <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	D3D12_VIEWPORT viewports[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]	= {};
	D3D12_RECT	   scissors[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]	    = {};

	for(int i=0;i<num;i++)
	{
		// Configure viewports
		viewports[i].Width		= (float)vps[i].w;
		viewports[i].Height		= (float)vps[i].h;
		viewports[i].TopLeftX	= (float)vps[i].x;
		viewports[i].TopLeftY	= (float)vps[i].y;
		viewports[i].MinDepth	= 0.0f;
		viewports[i].MaxDepth	= 1.0f;

		// Configure scissor
		scissors[i].left		= (LONG)viewports[i].TopLeftX;
		scissors[i].top			= (LONG)viewports[i].TopLeftY;
		scissors[i].right		= (LONG)viewports[i].TopLeftX+(LONG)viewports[i].Width;
		scissors[i].bottom		= (LONG)viewports[i].TopLeftY+(LONG)viewports[i].Height;
	}

	commandList->RSSetViewports(num, viewports);
	commandList->RSSetScissorRects(num, scissors);

    // This call will ensure that we cache the viewport change inside 
    // the target stack:
	crossplatform::RenderPlatform::SetViewports(deviceContext,num,vps);
}

void RenderPlatform::SetIndexBuffer(crossplatform::GraphicsDeviceContext &deviceContext,const crossplatform::Buffer *buffer)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context=deviceContext.platform_context;

	auto pBuffer = (dx12::Buffer*)buffer;

	commandList->IASetIndexBuffer(pBuffer->GetIndexBufferView());
}

static D3D_PRIMITIVE_TOPOLOGY toD3dTopology(crossplatform::Topology t)
{	
	using namespace crossplatform;
	switch(t)
	{			
	case Topology::POINTLIST:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case Topology::LINELIST:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case Topology::LINESTRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case Topology::TRIANGLELIST:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case Topology::TRIANGLESTRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case Topology::LINELIST_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case Topology::LINESTRIP_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case Topology::TRIANGLELIST_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case Topology::TRIANGLESTRIP_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	default:
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};
}

void RenderPlatform::SetTopology(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Topology t)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context=deviceContext.platform_context;

	D3D_PRIMITIVE_TOPOLOGY T = toD3dTopology(t);
	commandList->IASetPrimitiveTopology(T);
	mStoredTopology = T;
}

void RenderPlatform::SetLayout(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Layout *l)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context=deviceContext.platform_context;

	if (l)
	{
		l->Apply(deviceContext);
	}
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext& deviceContext,const crossplatform::RenderState* s)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	//immediateContext.platform_context   = deviceContext.platform_context;
    dx12::RenderState* state            = (dx12::RenderState*)s;
    // We cache the description, during EffectPass::Apply() we will check if the PSO
    // needs to be recreated
	if (s->type == crossplatform::BLEND)
	{
        BlendStateOverride = &state->BlendDesc;
	}
	else if (s->type == crossplatform::DEPTH)
	{
        DepthStateOverride = &state->DepthStencilDesc;
	}
    else if (s->type == crossplatform::RASTERIZER)
    {
        RasterStateOverride = &state->RasterDesc;
    }
    else
    {
        SIMUL_CERR << "Provided an invalid render state \n";
    }
}

void RenderPlatform::Resolve(crossplatform::GraphicsDeviceContext& deviceContext,crossplatform::Texture* destination,crossplatform::Texture* source)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
    //immediateContext.platform_context   = commandList;
    dx12::Texture* src                  = (dx12::Texture*)source;
    dx12::Texture* dst                  = (dx12::Texture*)destination;
    if (!src || !dst)
    {
        SIMUL_CERR << "Failed to Resolve.\n";
        return;
    }

    DXGI_FORMAT resolveFormat = {};
    // Both are typed
    if (IsTypeless(src->dxgi_format,false) && !IsTypeless(dst->dxgi_format,false))
    {
        resolveFormat = src->dxgi_format;
    }

    
    // NOTE: we resolve from/to mip = 0 & index = 0
    // Transition the resources:
    auto srcInitState = src->GetCurrentState(deviceContext,0,0);
    ResourceTransitionSimple(deviceContext,src->AsD3D12Resource(), srcInitState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, true);
    auto dstInitState = dst->GetCurrentState(deviceContext,0,0);
    ResourceTransitionSimple(deviceContext,dst->AsD3D12Resource(), dstInitState, D3D12_RESOURCE_STATE_RESOLVE_DEST, true);
    {
        commandList->ResolveSubresource(dst->AsD3D12Resource(),0,src->AsD3D12Resource(),0,src->dxgi_format);
    }
    // Revert states:
    ResourceTransitionSimple(deviceContext,src->AsD3D12Resource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcInitState);
    ResourceTransitionSimple(deviceContext,dst->AsD3D12Resource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, dstInitState);
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext& deviceContext, bool error_checking)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();

	crossplatform::ContextState *cs = GetContextState(deviceContext);
	if (!cs || !cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}

	dx12::EffectPass* pass  = static_cast<dx12::EffectPass*>(cs->currentEffectPass);
	if (!cs->effectPassValid)
	{
		if (cs->last_action_was_compute&&pass->shaders[crossplatform::SHADERTYPE_VERTEX] != nullptr)
		{
			cs->last_action_was_compute = false;
		}
		else if ((!cs->last_action_was_compute) && pass->shaders[crossplatform::SHADERTYPE_COMPUTE] != nullptr)
		{
			cs->last_action_was_compute = true;
		}
		// This applies the pass, and also any associated state: Blend, Depth and Rasterizer states:
		pass->Apply(deviceContext, cs->last_action_was_compute);
		cs->effectPassValid = true;
	}
	
	// We will only set the tables once per frame
	if (mLastFrame != deviceContext.frame_number)
	{
		// Call start render at least once per frame to make sure the bins 
		// release objects!
		BeginD3D12Frame();

		mLastFrame = deviceContext.frame_number;
		mCurIdx++;
		mCurIdx = mCurIdx % kNumIdx;

		// Reset the frame heaps (SRV_CBV_UAV and SAMPLER)
		mFrameHeap[mCurIdx].Reset();
        // Reset the override samplers heap
        mFrameOverrideSamplerHeap[mCurIdx].Reset();
	}

	auto cmdList    = deviceContext.asD3D12Context();
    auto dx12Effect = (dx12::Effect*)cs->currentEffect;

	// Set the frame descriptor heaps
    Heap* currentSamplerHeap                = dx12Effect->GetEffectSamplerHeap();
    ID3D12DescriptorHeap* currentHeaps[2]   = { mFrameHeap[mCurIdx].GetHeap(),currentSamplerHeap->GetHeap()};
    // If we are overriding samplers, use the override heap instead:
    if (cs->samplerStateOverrides.size() > 0)
    {
        currentSamplerHeap  = &mFrameOverrideSamplerHeap[mCurIdx];
        currentHeaps[1]     = currentSamplerHeap->GetHeap();
    }
	cmdList->SetDescriptorHeaps(2, currentHeaps);

	// Apply the RootDescriptor tables:
	// NOTE: we define our RootSignature on HLSl and then, using SFX we attatch it to
	//       the shader binary, refer to Simul/Platform/<target>/HLSL/GFX.hls
	const UINT cbvSrvUavTableId = 0;
	const UINT samplerTableId	= 1;
	auto bufferGpuHandle=mFrameHeap[mCurIdx].GpuHandle();
	auto samplerGpuHandle=currentSamplerHeap->GpuHandle();
	if (pass->IsCompute())
	{
		cmdList->SetComputeRootSignature(mGRootSignature);
		cmdList->SetComputeRootDescriptorTable(cbvSrvUavTableId, mFrameHeap[mCurIdx].GpuHandle());
        cmdList->SetComputeRootDescriptorTable(samplerTableId, currentSamplerHeap->GpuHandle());
	}
	else if(pass->IsRaytrace())
	{
		cmdList->SetComputeRootSignature(mGRaytracingGlobalSignature);
		cmdList->SetComputeRootDescriptorTable(cbvSrvUavTableId,bufferGpuHandle);
        cmdList->SetComputeRootDescriptorTable(samplerTableId, samplerGpuHandle);
	}
	else
	{
		// Apply the common root signature
		cmdList->SetGraphicsRootSignature(mGRootSignature);
		cmdList->SetGraphicsRootDescriptorTable(cbvSrvUavTableId, mFrameHeap[mCurIdx].GpuHandle());
        cmdList->SetGraphicsRootDescriptorTable(samplerTableId, currentSamplerHeap->GpuHandle());
	}

	// Apply Override samplers:	
    if (cs->samplerStateOverrides.size() > 0)
    {
	    pass->SetSamplers(dx12Effect->GetSamplers(),&mFrameOverrideSamplerHeap[mCurIdx],m12Device,deviceContext);
    }
	
	// Apply CBVs:
	pass->SetConstantBuffers(cs->applyBuffers, &mFrameHeap[mCurIdx],m12Device,deviceContext);

	// Apply SRVs (textures and SB):
	pass->SetSRVs(cs->textureAssignmentMap, cs->applyStructuredBuffers, &mFrameHeap[mCurIdx], m12Device, deviceContext);

	// Apply UAVs (RwTextures and RwSB):
	pass->SetUAVs(cs->rwTextureAssignmentMap, cs->applyRwStructuredBuffers, &mFrameHeap[mCurIdx], m12Device, deviceContext);

	// We are ready to draw/dispatch, so now flush the barriers!
	FlushBarriers(deviceContext);

    // We are now done with this states
    DepthStateOverride  = nullptr;
    BlendStateOverride  = nullptr;
    RasterStateOverride = nullptr;
	
	if(pass->IsRaytrace())
	{
	}
	return true;
}

void RenderPlatform::DrawQuad(crossplatform::GraphicsDeviceContext &deviceContext)
{
	ID3D12GraphicsCommandList*	commandList		= deviceContext.asD3D12Context();
	SetTopology(deviceContext,simul::crossplatform::Topology::TRIANGLESTRIP);
	ApplyContextState(deviceContext);
	commandList->DrawInstanced(4, 1, 0, 0);
	//Signal(deviceContext, generalFence, generalFenceVal++);
}


crossplatform::Shader *RenderPlatform::CreateShader()
{
	Shader *S = new Shader();
	return S;
}

crossplatform::BottomLevelAccelerationStructure* RenderPlatform::CreateBottomLevelAccelerationStructure()
{
	return new BottomLevelAccelerationStructure(this);
}

crossplatform::TopLevelAccelerationStructure* RenderPlatform::CreateTopLevelAccelerationStructure()
{
	return new TopLevelAccelerationStructure(this);
}

crossplatform::ShaderBindingTable* RenderPlatform::CreateShaderBindingTable()
{
	return new ShaderBindingTable();
}

crossplatform::DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
    return new dx12::DisplaySurface();
}


crossplatform::GpuProfiler* RenderPlatform::CreateGpuProfiler()
{
	return new dx12::GpuProfiler();
}