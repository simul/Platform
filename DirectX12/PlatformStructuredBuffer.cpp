#include "PlatformStructuredBuffer.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/Core/StringToWString.h"
using namespace simul;
using namespace dx12;

PlatformStructuredBuffer::PlatformStructuredBuffer():
    mGPUBuffer(nullptr),
    mUploadBuffer(nullptr),
	mReadSrc(nullptr),
	mTempBuffer(nullptr),
	mChanged(false),
	mNumElements(0),
	mElementByteSize(0),
    mCpuRead(false),
    mCurApplies(0),
    mLastFrame(UINT64_MAX)
{
    for (int i = 0; i < mBuffering; i++)
    {
        mReadBuffers[i] = nullptr;
    }
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r, int ct, int unit_size, bool computable,bool cpu_read, void *init_data,const char *n, crossplatform::BufferUsageHint b)
{
	HRESULT res			= S_FALSE;
    if(n)
        name=n;
	mNumElements		= ct;
	mElementByteSize	= unit_size;
	
    mUnitSize           = mNumElements * mElementByteSize;
    mTotalSize			= mUnitSize * mMaxApplyMod;

    renderPlatform                          = r;
	dx12::RenderPlatform* mRenderPlatform	= (dx12::RenderPlatform*)renderPlatform;
	mCpuRead                                = cpu_read;

	if (mTotalSize <= 0)
	{
        SIMUL_INTERNAL_CERR << "The size of a Structured Buffer can not be 0 \n";
        return;
	}

    // size_t totalGpuBytes = mTotalSize;
    // SIMUL_COUT << "Allocating GPU memory for Structured Buffer: " << totalGpuBytes << " (" << (float)totalGpuBytes / 1024.0f / 1024.0f << ")\n";

    // Create the buffers:
    //  const D3D12_RESOURCE_STATES		mShaderResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_RESOURCE_STATES initState     = computable ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS        : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_FLAGS bufferFlags    = computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS   : D3D12_RESOURCE_FLAG_NONE;
    mCurrentState                       = initState;

    // Default heap:
    res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
    (
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mTotalSize, bufferFlags),
        initState,
        nullptr,
        SIMUL_PPV_ARGS(&mGPUBuffer)
    );
    SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mGPUBuffer, mTotalSize)
    std::wstring wstr=base::StringToWString(name);
    mGPUBuffer->SetName((wstr+L" GPU_SB").c_str());

    // Upload heap:
    res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
    (
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        SIMUL_PPV_ARGS(&mUploadBuffer)
    );
    SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mUploadBuffer, mTotalSize)
    mUploadBuffer->SetName((wstr+L" CPU_SB").c_str());

    // If provided data, init the GPU buffer with it:
    if (init_data)
    {
        void* pNewData = malloc(mTotalSize);
        memset(pNewData, 0, mTotalSize);
        memcpy(pNewData, init_data, mUnitSize);
        
        D3D12_SUBRESOURCE_DATA dataToCopy   = {};
        dataToCopy.pData                    = pNewData;
        dataToCopy.RowPitch                 = dataToCopy.SlicePitch = mUnitSize;
        crossplatform::DeviceContext &deviceContext=mRenderPlatform->GetImmediateContext();
        mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST, true);
        UpdateSubresources(deviceContext.asD3D12Context(), mGPUBuffer, mUploadBuffer, 0, 0, 1, &dataToCopy);
        mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState, true);
        
        free(pNewData);
    }

    // If this Structured Buffer supports CPU read,
    // we initialize a set of READ_BACK buffers:
    if (mCpuRead)
    {
        for (unsigned int i = 0; i < mBuffering; i++)
        {
            res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
            (
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                SIMUL_PPV_ARGS(&mReadBuffers[i])
            );
            SIMUL_ASSERT(res == S_OK);
            wchar_t wc=L'0'+(wchar_t)i;
            mReadBuffers[i]->SetName(((wstr+L" READ_SB ")+&wc).c_str());
			SIMUL_GPU_TRACK_MEMORY(mReadBuffers[i], mTotalSize)
        }
    }

	// Create the SR views:
	mBufferSrvHeap.Restore(mRenderPlatform, mMaxApplyMod, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBSrvHeap", false);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements				= mNumElements;
	srvDesc.Buffer.StructureByteStride		= mElementByteSize;
	srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;

    mSrvViews.resize(mMaxApplyMod);
    for (int v = 0; v < mMaxApplyMod; v++)
    {
	    srvDesc.Buffer.FirstElement	= v * mNumElements;

	    mRenderPlatform->AsD3D12Device()->CreateShaderResourceView(mGPUBuffer, &srvDesc, mBufferSrvHeap.CpuHandle());
	    mSrvViews[v] = mBufferSrvHeap.CpuHandle();
	    mBufferSrvHeap.Offset();
    }

	// If requested, create UA views:
	if (computable)
	{
		mBufferUavHeap.Restore(mRenderPlatform, mMaxApplyMod, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBUavHeap", false);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
		uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format								= DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.CounterOffsetInBytes			= 0;
		uavDesc.Buffer.NumElements					= mNumElements;
		uavDesc.Buffer.StructureByteStride			= mElementByteSize;
		uavDesc.Buffer.Flags						= D3D12_BUFFER_UAV_FLAG_NONE;
        
        mUavViews.resize(mMaxApplyMod);
        for (int v = 0; v < mMaxApplyMod; v++)
        {
            uavDesc.Buffer.FirstElement = v * mNumElements;

		    mRenderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mGPUBuffer, nullptr, &uavDesc, mBufferUavHeap.CpuHandle());
		    mUavViews[v] = mBufferUavHeap.CpuHandle();
		    mBufferUavHeap.Offset();
        }
	}

	// Create a temporal buffer that we will use to upload data to the GPU
    mTempBuffer = reinterpret_cast<void*>(new char[mUnitSize]);
	memset(mTempBuffer, 0, mUnitSize);
    if (init_data)
    {
        memcpy(mTempBuffer, init_data, mUnitSize);
    }
}


void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext,  const crossplatform::ShaderResource &shaderResource)
{
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext,  shaderResource);

    // Reset the current applies, we need to do this even if we didnt change the buffer:
    if (mLastFrame != deviceContext.frame_number)
    {
        mLastFrame = deviceContext.frame_number;
		mFrameCycle++;
		if(mFrameCycle>2)
		{
			mFrameCycle=0;
			mCurApplies = 0;
		}
    }

	// If it changed (GetBuffer() was called) we will upload the data in the temp buffer to the GPU
	if (mChanged||mCurApplies >= mMaxApplyMod)
	{
        UpdateBuffer(deviceContext);
	}
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource &shaderResource)
{
    crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext,  shaderResource);

    // Reset the current applies, we need to do this even if we didnt change the buffer:
    if (mLastFrame != deviceContext.frame_number)
    {
        mLastFrame = deviceContext.frame_number;
		mFrameCycle++;
		if(mFrameCycle>2)
		{
			mFrameCycle=0;
			mCurApplies = 0;
		}
    }

    // If it changed (GetBuffer() was called) we will upload the data in the temp buffer to the GPU
    if (mChanged||mCurApplies >= mMaxApplyMod)
    {
        UpdateBuffer(deviceContext);
    }
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &)
{
	if (mChanged)
	{
		SIMUL_CERR_ONCE << "You must Apply this SB before calling GetBuffer again! " << std::endl;
		return nullptr;
	}
	mChanged = true;
	return mTempBuffer;
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map
	// We intend to read from CPU so we pass a valid range!
    // We read from the oldest buffer
	const CD3DX12_RANGE readRange(0, 1);
	unsigned int curIdx = (deviceContext.frame_number + 1) % mBuffering;
    if(!mReadBuffers[curIdx])
        return nullptr;
    HRESULT hr=mReadBuffers[curIdx]->Map(0, &readRange, reinterpret_cast<void**>(&mReadSrc));
	if(hr!=S_OK)
	{
		SIMUL_INTERNAL_CERR<<"Failed to map PlatformStructuredBuffer for reading."<<std::endl;
		SIMUL_BREAK_ONCE("Failed here");
		return nullptr;
	}
	return mReadSrc;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	// We are mapping from a readback buffer so it doesnt matter what we modified,
	// the GPU won't have acces to it. We pass a 0,0 range here.
	const CD3DX12_RANGE readRange(0, 0);
	unsigned int curIdx = (deviceContext.frame_number + 1) % mBuffering;
    if(mReadBuffers[curIdx])
    	mReadBuffers[curIdx]->Unmap(0, &readRange);
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();
	unsigned int curIdx                     = deviceContext.frame_number % mBuffering;
	dx12::RenderPlatform *mRenderPlatform = static_cast<dx12::RenderPlatform*>(renderPlatform);

	// Check state
	bool changed = false;
	if ((mCurrentState & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changed = true;
		mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	}

	// Schedule a copy
    commandList->CopyBufferRegion(mReadBuffers[curIdx], 0, mGPUBuffer, 0, mUnitSize);
	
    // Restore state
	if (changed)
	{
		mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, mCurrentState,true);
	}
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext&deviceContext ,void* data)
{
	if (data)
	{
        void* pNewData = malloc(mTotalSize);
        memset(pNewData, 0, mTotalSize);
        memcpy(pNewData, data, mUnitSize);

        D3D12_SUBRESOURCE_DATA dataToCopy = {};
        dataToCopy.pData = pNewData;
        dataToCopy.RowPitch = dataToCopy.SlicePitch = mUnitSize;
		
	    ID3D12GraphicsCommandList*	commandList = deviceContext.asD3D12Context();
		dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
        mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST, true);
        UpdateSubresources(commandList, mGPUBuffer, mUploadBuffer, 0, 0, 1, &dataToCopy);
        mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState, true);

        free(pNewData);
	}
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if (mTempBuffer)
	{
		delete[] mTempBuffer;
		mTempBuffer = nullptr;
	}
	dx12::RenderPlatform *mRenderPlatform = static_cast<dx12::RenderPlatform*>(renderPlatform);
	mBufferSrvHeap.Release();
	mBufferUavHeap.Release();
    SAFE_RELEASE_LATER(mUploadBuffer);
    SAFE_RELEASE_LATER(mGPUBuffer);
	for (unsigned int i = 0; i < mBuffering; i++)
	{
        SAFE_RELEASE_LATER(mReadBuffers[i]);
	}
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& )
{
}

void PlatformStructuredBuffer::UpdateBuffer(simul::crossplatform::DeviceContext& deviceContext)
{
	auto r  = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	if (mChanged)
	{
		mCurApplies++;
		mChanged = false;
	}

    // We need to recreate the internal buffers:
    if (mCurApplies >= mMaxApplyMod)
    {
        void* pCacheData = malloc(mUnitSize);
        memcpy(pCacheData, mTempBuffer, mUnitSize);

        mMaxApplyMod    = mMaxApplyMod * 2 + 50;
        bool isUav      = !mUavViews.empty();
        InvalidateDeviceObjects();
        
        SIMUL_COUT << name.c_str()<<": resizing Structured Buffer(" << mMaxApplyMod << ")\n";
        RestoreDeviceObjects(deviceContext.renderPlatform, mNumElements, mElementByteSize, isUav, mCpuRead, pCacheData,name.c_str(), bufferUsageHint);
        
        free(pCacheData);
        mCurApplies     = 0;
        r->FlushBarriers(deviceContext);
    }

    // First update the UPLOAD buffer at the apply offset:
    UINT8* pBuffer  = nullptr;
    UINT curOff     = mCurApplies * mUnitSize;
    const CD3DX12_RANGE readRange(0, 0);
    HRESULT res = mUploadBuffer->Map(0, &readRange, (void**)&pBuffer);
	if (res != S_OK)
	{
		SIMUL_BREAK_ONCE("Failed to map buffer.");
		return;
	}
    memcpy(pBuffer + curOff, mTempBuffer, mUnitSize);
    mUploadBuffer->Unmap(0, nullptr);
    
    // Now copy the updated region from the UPLOAD to the DEFAULT buffer:
    r->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST, true);
	ID3D12GraphicsCommandList*	commandList                        = deviceContext.asD3D12Context();
    commandList->CopyBufferRegion(mGPUBuffer, curOff, mUploadBuffer, curOff, mUnitSize);
    r->ResourceTransitionSimple(deviceContext,mGPUBuffer, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState, true);
    
    //... we won't increase the apply count until we get the view
    //... we will set mChange to false when we get the view
}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12ShaderResourceView(crossplatform::DeviceContext&deviceContext )
{
	dx12::RenderPlatform *mRenderPlatform = static_cast<dx12::RenderPlatform*>(renderPlatform);
    
	// Check the resource state
	bool is_pixel_shader=(deviceContext.contextState.currentEffectPass->shaders[crossplatform::SHADERTYPE_PIXEL]!=nullptr);
     D3D12_RESOURCE_STATES		readState = is_pixel_shader?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (mCurrentState != readState)
	{
		mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, readState);
        mCurrentState = readState;
	}
	
    if (mCurApplies < mMaxApplyMod)
    {
		// Return the current view
		D3D12_CPU_DESCRIPTOR_HANDLE* view = &mSrvViews[mCurApplies];
		return view;
	}
	else
	{
		SIMUL_INTERNAL_CERR << "Reached the maximum apply for this SB!\n";
    }
	// TODO: this is wrong. Can it ever happen?
	return &mSrvViews[0];
}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12UnorderedAccessView(crossplatform::DeviceContext&deviceContext ,int mip /*= 0*/)
{
	dx12::RenderPlatform *mRenderPlatform = static_cast<dx12::RenderPlatform*>(renderPlatform);

	// Check the resource state
	if (mCurrentState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		mRenderPlatform->ResourceTransitionSimple(deviceContext,mGPUBuffer, mCurrentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCurrentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

    // Return the current view
    D3D12_CPU_DESCRIPTOR_HANDLE* view = &mUavViews[0];
    if (mCurApplies >= mMaxApplyMod)
    {
        SIMUL_INTERNAL_CERR << "Reached the maximum apply for this SB! \n";
    }
    else
    {
        view = &mUavViews[mCurApplies];
    }
    return view;
}

ID3D12Resource *PlatformStructuredBuffer::AsD3D12Resource(crossplatform::DeviceContext &deviceContext)
{
    return mGPUBuffer;
}

void PlatformStructuredBuffer::ActualApply(simul::crossplatform::DeviceContext& , EffectPass* , int )
{
}
