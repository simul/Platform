#define NOMINMAX

#include "Effect.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/FileLoader.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "SimulDirectXHeader.h"
#include "MacrosDx1x.h"

#include <algorithm>
#include <string>

using namespace simul;
using namespace dx12;

#pragma optimize("",off)

inline bool IsPowerOfTwo( UINT64 n )
{
    return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
    SIMUL_ASSERT( IsPowerOfTwo(multiple) );

    return (value + multiple - 1) & ~(multiple - 1);
}

template< class T >
UINT64 BytePtrToUint64( _In_ T* ptr )
{
    return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}

Query::Query(crossplatform::QueryType t):
	crossplatform::Query(t),
	mQueryHeap(nullptr),
	mReadBuffer(nullptr),
	mQueryData(nullptr),
	mTime(0),
	mIsDisjoint(false)
{
	mD3DType = dx12::RenderPlatform::ToD3dQueryType(t);
	if (t == crossplatform::QueryType::QUERY_TIMESTAMP_DISJOINT)
	{
		mIsDisjoint = true;
	}
}

Query::~Query()
{
	InvalidateDeviceObjects();
}

void Query::SetName(const char *name)
{
	std::string n (name);
	mQueryHeap->SetName(std::wstring(n.begin(),n.end()).c_str());
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	InvalidateDeviceObjects();
	auto rPlat = (dx12::RenderPlatform*)r;

	// Create a query heap
	HRESULT res					= S_FALSE;
	mD3DType					= dx12::RenderPlatform::ToD3dQueryType(type);
	D3D12_QUERY_HEAP_DESC hDesc = {};
	hDesc.Count					= QueryLatency;
	hDesc.NodeMask				= 0;
	hDesc.Type					= dx12::RenderPlatform::ToD3D12QueryHeapType(type);
	res							= r->AsD3D12Device()->CreateQueryHeap(&hDesc, SIMUL_PPV_ARGS(&mQueryHeap));
	SIMUL_ASSERT(res == S_OK);

	// Create a redback buffer to get data
	res = rPlat->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(QueryLatency * sizeof(UINT64)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
        SIMUL_PPV_ARGS(&mReadBuffer)
	);
	SIMUL_ASSERT(res == S_OK);
	mReadBuffer->SetName(L"QueryReadBuffer");
}

void Query::InvalidateDeviceObjects() 
{
	SAFE_RELEASE(mQueryHeap);
	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i]= true;	// ?????????
		doneQuery[i] = false;
	}
	SAFE_RELEASE(mReadBuffer);
	mQueryData = nullptr;
}

void Query::Begin(crossplatform::DeviceContext &deviceContext)
{

}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
	gotResults[currFrame] = false;
	doneQuery[currFrame]  = true;

	// For now, only take into account time stamp queries
	if (mD3DType != D3D12_QUERY_TYPE_TIMESTAMP)
	{
		return;
	}

	deviceContext.asD3D12Context()->EndQuery
	(
		mQueryHeap,
		mD3DType,
		currFrame
	);

	deviceContext.asD3D12Context()->ResolveQueryData
	(
		mQueryHeap, mD3DType,
		currFrame, 1,
		mReadBuffer, currFrame * sizeof(UINT64)
	);
}

bool Query::GetData(crossplatform::DeviceContext& deviceContext,void *data,size_t sz)
{
	gotResults[currFrame]	= true;
	currFrame				= (currFrame + 1) % QueryLatency;

	// We provide frequency information
	if (mIsDisjoint)
	{
		auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
		crossplatform::DisjointQueryStruct disjointData;
		disjointData.Disjoint	= false;
		disjointData.Frequency	= rPlat->GetTimeStampFreq();
		memcpy(data, &disjointData, sz);
		return true;
	}

	// For now, only take into account time stamp queries
	if (mD3DType != D3D12_QUERY_TYPE_TIMESTAMP)
	{
		const UINT64 invalid = 0;
		data = (void*)invalid;
		return true;
	}

	if (!doneQuery[currFrame])
	{
		return false;
	}
	
	// Get the values from the buffer
	HRESULT res				= S_FALSE;
	res						= mReadBuffer->Map(0, &CD3DX12_RANGE(0, 1), reinterpret_cast<void**>(&mQueryData));
	SIMUL_ASSERT(res == S_OK);
	{
		UINT64* d			= (UINT64*)mQueryData;
		UINT64 time			= d[currFrame];
		mTime				= time;
		memcpy(data, &mTime, sz);
	}
	mReadBuffer->Unmap(0, &CD3DX12_RANGE(0, 0));

	return true;
} 

RenderState::RenderState()
{
	BlendDesc			                = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RasterDesc			                = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	DepthStencilDesc	                = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RasterDesc.FrontCounterClockwise    = true;	
}

RenderState::~RenderState()
{
}

RenderTargetState::RenderTargetState() :
    Num(0)
{
    DepthStencilFmt = crossplatform::PixelFormat::UNKNOWN;
    for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        ColourFmts[i] = DepthStencilFmt;
    }
}

bool RenderTargetState::IsDepthEnabled()
{
    return DepthStencilFmt != crossplatform::PixelFormat::UNKNOWN;
}

int RenderTargetState::GetHash()
{
    size_t h = 0;
    h       |= unsigned(Num << 0);
    h       |= unsigned(DepthStencilFmt << 8);
    for (int i = 0; i < Num; i++) // we only account for used formats
    {
        h   |= unsigned(ColourFmts[i] << (16 + (8 * i)));
    }
    return h;
}

PlatformStructuredBuffer::PlatformStructuredBuffer():
	mNumElements(0),
	mElementByteSize(0),
	mChanged(false),
	mReadSrc(nullptr),
	mTempBuffer(nullptr),
    mCpuRead(false),
    mCurApplies(0),
    mLastFrame(UINT64_MAX)
{
    for (int i = 0; i < mBuffering; i++)
    {
        mGPUBuffers[i] = mUploadBuffers[i] = mReadBuffers[i] = nullptr;
        mSrvViews[i] = mUavViews[i] = nullptr;
    }
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r, int ct, int unit_size, bool computable,bool cpu_read, void *init_data)
{
	HRESULT res			= S_FALSE;
	mNumElements		= ct;
	mElementByteSize	= unit_size;
	
    mUnitSize           = mNumElements * mElementByteSize;
    mTotalSize			= mUnitSize * mMaxApplyMod;

    renderPlatform      = r;
	dx12::RenderPlatform *mRenderPlatform		= (dx12::RenderPlatform*)renderPlatform;
	mCpuRead            = cpu_read;

	if (mTotalSize <= 0)
	{
		SIMUL_BREAK("You are creating a StructuredBuffer of size 0");
	}

    // Create the buffers:
    D3D12_RESOURCE_STATES initState     = computable ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS        : mShaderResourceState;
	D3D12_RESOURCE_FLAGS bufferFlags    = computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS   : D3D12_RESOURCE_FLAG_NONE;
    for (unsigned int i = 0; i < mBuffering; i++)
    {
        mCurrentState[i] = initState;

        // Default heap:
        res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(mTotalSize, bufferFlags),
            initState,
            nullptr,
            SIMUL_PPV_ARGS(&mGPUBuffers[i])
        );
        SIMUL_ASSERT(res == S_OK);
        mGPUBuffers[i]->SetName(L"SBDefaultBuffer");

        // Upload heap:
        res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            SIMUL_PPV_ARGS(&mUploadBuffers[i])
        );
        SIMUL_ASSERT(res == S_OK);
        mUploadBuffers[i]->SetName(L"SBUploadBuffer");

        // If provided data, init the GPU buffer with it:
        if (init_data)
        {
            void* pNewData = malloc(mTotalSize);
            memset(pNewData, 0, mTotalSize);
            memcpy(pNewData, init_data, mUnitSize);

            D3D12_SUBRESOURCE_DATA dataToCopy   = {};
            dataToCopy.pData                    = pNewData;
            dataToCopy.RowPitch                 = dataToCopy.SlicePitch = mUnitSize;

            mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[i], mCurrentState[i], D3D12_RESOURCE_STATE_COPY_DEST, true);
            UpdateSubresources(mRenderPlatform->AsD3D12CommandList(), mGPUBuffers[i], mUploadBuffers[i], 0, 0, 1, &dataToCopy);
            mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[i], D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState[i], true);

            free(pNewData);
        }

        // If this Structured Buffer supports CPU read,
        // we initialize a set of READ_BACK buffers:
        if (mCpuRead)
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
            mReadBuffers[i]->SetName(L"SBReadbackBuffer");
        }
    }

	// Create the SR views:
	mBufferSrvHeap.Restore(mRenderPlatform, mBuffering * mMaxApplyMod, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBSrvHeap", false);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements				= mNumElements;
	srvDesc.Buffer.StructureByteStride		= mElementByteSize;
	srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;
    for (unsigned int i = 0; i < mBuffering; i++)
    {
        mSrvViews[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[mMaxApplyMod];

        for (int v = 0; v < mMaxApplyMod; v++)
        {
	        srvDesc.Buffer.FirstElement	= v * mNumElements;

	        mRenderPlatform->AsD3D12Device()->CreateShaderResourceView(mGPUBuffers[i], &srvDesc, mBufferSrvHeap.CpuHandle());
	        mSrvViews[i][v] = mBufferSrvHeap.CpuHandle();
	        mBufferSrvHeap.Offset();
        }
    }

	// If requested, create UA views:
	if (computable)
	{
		mBufferUavHeap.Restore(mRenderPlatform, mBuffering * mMaxApplyMod, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBUavHeap", false);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
		uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format								= DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.CounterOffsetInBytes			= 0;
		uavDesc.Buffer.NumElements					= mNumElements;
		uavDesc.Buffer.StructureByteStride			= mElementByteSize;
		uavDesc.Buffer.Flags						= D3D12_BUFFER_UAV_FLAG_NONE;
        for (unsigned int i = 0; i < mBuffering; i++)
        {
            mUavViews[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[mMaxApplyMod];
            
            for (int v = 0; v < mMaxApplyMod; v++)
            {
                uavDesc.Buffer.FirstElement = v * mNumElements;

		        mRenderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mGPUBuffers[i], nullptr, &uavDesc, mBufferUavHeap.CpuHandle());
		        mUavViews[i][v] = mBufferUavHeap.CpuHandle();
		        mBufferUavHeap.Offset();
            }
        }   
	}

	// Create a temporal buffer that we will use to upload data to the GPU
	mTempBuffer = malloc(mTotalSize);
	memset(mTempBuffer, 0, mTotalSize);
}


void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect *effect, const crossplatform::ShaderResource &shaderResource)
{
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext, effect, shaderResource);

    // Reset the current applies, we need to do this even if we didnt change the buffer:
    if (mLastFrame != deviceContext.frame_number)
    {
        mLastFrame = deviceContext.frame_number;
        mCurApplies = 0;
    }

	// If it changed (GetBuffer() was called) we will upload the data in the temp buffer to the GPU
	if (mChanged)
	{
        UpdateBuffer(deviceContext);
	}
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource &shaderResource)
{
    crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext, effect, shaderResource);

    // Reset the current applies, we need to do this even if we didnt change the buffer:
    if (mLastFrame != deviceContext.frame_number)
    {
        mLastFrame = deviceContext.frame_number;
        mCurApplies = 0;
    }

    // If it changed (GetBuffer() was called) we will upload the data in the temp buffer to the GPU
    if (mChanged)
    {
        UpdateBuffer(deviceContext);
    }
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &deviceContext)
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
    mReadBuffers[curIdx]->Map(0, &readRange, reinterpret_cast<void**>(&mReadSrc));
	return mReadSrc;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	// We are mapping from a readback buffer so it doesnt matter what we modified,
	// the GPU won't have acces to it. We pass a 0,0 range here.
	const CD3DX12_RANGE readRange(0, 0);
	unsigned int curIdx = (deviceContext.frame_number + 1) % mBuffering;
	mReadBuffers[curIdx]->Unmap(0, &readRange);
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	unsigned int curIdx = deviceContext.frame_number % mBuffering;

	
	dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
	// Check state
	bool changed = false;
	if ((mCurrentState[curIdx] & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changed = true;
		mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[curIdx], mCurrentState[curIdx], D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	}

	// Schedule a copy
    UINT64 dataPortion = (mTotalSize + 1) / mMaxApplyMod;
    deviceContext.renderPlatform->AsD3D12CommandList()->CopyBufferRegion(mReadBuffers[curIdx], 0, mGPUBuffers[curIdx], 0, dataPortion);
	
    // Restore state
	if (changed)
	{
		mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[curIdx], D3D12_RESOURCE_STATE_COPY_SOURCE, mCurrentState[curIdx],true);
	}
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext,void* data)
{
	if (data)
	{
        void* pNewData = malloc(mTotalSize);
        memset(pNewData, 0, mTotalSize);
        memcpy(pNewData, data, mUnitSize);

        D3D12_SUBRESOURCE_DATA dataToCopy = {};
        dataToCopy.pData = pNewData;
        dataToCopy.RowPitch = dataToCopy.SlicePitch = mUnitSize;
		
		dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
        for (unsigned int i = 0; i < mBuffering; i++)
        {
            mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[i], mCurrentState[i], D3D12_RESOURCE_STATE_COPY_DEST, true);
            UpdateSubresources(mRenderPlatform->AsD3D12CommandList(), mGPUBuffers[i], mUploadBuffers[i], 0, 0, 1, &dataToCopy);
            mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[i], D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState[i], true);
        }

        free(pNewData);
	}
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if (mTempBuffer)
	{
		delete mTempBuffer;
		mTempBuffer = nullptr;
	}
	dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
	mBufferSrvHeap.Release(mRenderPlatform);
	mBufferUavHeap.Release(mRenderPlatform);
	for (unsigned int i = 0; i < mBuffering; i++)
	{
        SAFE_DELETE(mSrvViews[i]);
        SAFE_DELETE(mUavViews[i]);
	    mRenderPlatform->PushToReleaseManager(mUploadBuffers[i], "SBUpload");
	    mRenderPlatform->PushToReleaseManager(mGPUBuffers[i], "SBDefault");
		mRenderPlatform->PushToReleaseManager(mReadBuffers[i], "SBRead");
	}
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::UpdateBuffer(simul::crossplatform::DeviceContext& deviceContext)
{
    int idx     = deviceContext.frame_number % mBuffering;
	auto r      = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	mChanged    = false;

    // First update the UPLOAD buffer at the apply offset:
    UINT8* pBuffer  = nullptr;
    UINT curOff     = mCurApplies * mUnitSize;
    const CD3DX12_RANGE readRange(0, 0);
    HRESULT res = mUploadBuffers[idx]->Map(0, &readRange, (void**)&pBuffer);
    memcpy(pBuffer + curOff, mTempBuffer, mUnitSize);
    mUploadBuffers[idx]->Unmap(0, nullptr);
    
    // Now copy the updated region from the UPLOAD to the DEFAULT buffer:
    r->ResourceTransitionSimple(mGPUBuffers[idx], mCurrentState[idx], D3D12_RESOURCE_STATE_COPY_DEST, true);
    r->AsD3D12CommandList()->CopyBufferRegion(mGPUBuffers[idx], curOff, mUploadBuffers[idx], curOff, mUnitSize);
    r->ResourceTransitionSimple(mGPUBuffers[idx], D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState[idx], true);
    
    //... we won't increse the apply count until we get the view
}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext)
{
    int idx = deviceContext.frame_number % mBuffering;
	dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
	// Check the resource state
	if (mCurrentState[idx] != mShaderResourceState)
	{
		mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[idx], mCurrentState[idx], mShaderResourceState);
        mCurrentState[idx] = mShaderResourceState;
	}
    // Safety check
    if (mCurApplies > mMaxApplyMod - 1)
    {
        SIMUL_CERR << "Reached the maximum apply for this SB! \n";
        return &mSrvViews[idx][0];
    }
	return &mSrvViews[idx][mCurApplies++];
}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12UnorderedAccessView(crossplatform::DeviceContext& deviceContext,int mip /*= 0*/)
{
    int idx = deviceContext.frame_number % mBuffering;
	dx12::RenderPlatform *mRenderPlatform		=static_cast<dx12::RenderPlatform*>(renderPlatform);
	// Check the resource state
	if (mCurrentState[idx] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		mRenderPlatform->ResourceTransitionSimple(mGPUBuffers[idx], mCurrentState[idx], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mCurrentState[idx] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
    // Safety check
    if (mCurApplies > mMaxApplyMod - 1)
    {
        SIMUL_CERR << "Reached the maximum apply for this SB! \n";
        return &mUavViews[idx][0];
    }
	return &mUavViews[idx][mCurApplies++];
}

void PlatformStructuredBuffer::ActualApply(simul::crossplatform::DeviceContext& deviceContext, EffectPass* currentEffectPass, int slot)
{
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

Effect::Effect():
    mSamplersHeap(nullptr)
{
}

EffectTechnique* Effect::CreateTechnique()
{
	return new dx12::EffectTechnique;
}

void Shader::load(crossplatform::RenderPlatform *renderPlatform, const char *filename_utf8, crossplatform::ShaderType t)
{
	simul::base::MemoryInterface* allocator = renderPlatform->GetMemoryInterface();
	simul::base::FileLoader* fileLoader		= simul::base::FileLoader::GetFileLoader();

	struct FileBlob
	{
		void*		pData;
		uint32_t	DataSize;
	};

	// Load the shader binary
	FileBlob shaderBlob;
	std::string filenameUtf8	= renderPlatform->GetShaderBinaryPath();
	filenameUtf8				+= "/";
	filenameUtf8				+= filename_utf8;
	fileLoader->AcquireFileContents(shaderBlob.pData, shaderBlob.DataSize, filenameUtf8.c_str(), false);
	if (!shaderBlob.pData)
	{
		// Some engines force filenames to lower case because reasons:
		std::transform(filenameUtf8.begin(), filenameUtf8.end(), filenameUtf8.begin(), ::tolower);
		fileLoader->AcquireFileContents(shaderBlob.pData, shaderBlob.DataSize, filenameUtf8.c_str(), false);
		if (!shaderBlob.pData)
		{
			return;
		}
	}

	// Copy shader
	type = t;
	if (t == crossplatform::SHADERTYPE_PIXEL)
	{
		D3DCreateBlob(shaderBlob.DataSize, &pixelShader12);
		memcpy(pixelShader12->GetBufferPointer(), shaderBlob.pData, pixelShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_VERTEX)
	{
		D3DCreateBlob(shaderBlob.DataSize, &vertexShader12);
		memcpy(vertexShader12->GetBufferPointer(), shaderBlob.pData, vertexShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_COMPUTE)
	{
		D3DCreateBlob(shaderBlob.DataSize, &computeShader12);
		memcpy(computeShader12->GetBufferPointer(), shaderBlob.pData, computeShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_GEOMETRY)
	{
		SIMUL_CERR << "Geometry shaders are not implemented in Dx12" << std::endl;
	}

	// Free the loaded memory
	fileLoader->ReleaseFileContents(shaderBlob.pData);
}

void Effect::EnsureEffect(crossplatform::RenderPlatform *r, const char *filename_utf8)
{
#ifndef _XBOX_ONE
	// We will only recompile if we are in windows
	// SFX will handle the "if changed"
	auto buildMode = r->GetShaderBuildMode();
	if ((buildMode & crossplatform::BUILD_IF_CHANGED) != 0)
	{
		std::string simulPath = std::getenv("SIMUL");

		if (simulPath != "")
		{
			// Sfx path
			std::string exe = simulPath;
			exe += "\\Tools\\bin\\Sfx.exe";
			std::wstring sfxPath(exe.begin(), exe.end());

			/*
			Command line:
				argv[0] we need to pass the module name
				<input.sfx>
				-I <include path; include path>
				-O <output>
				-P <config.json>
			*/
			std::string cmdLine = exe;
			{
				// File to compile
				cmdLine += " " + simulPath + "\\Platform\\CrossPlatform\\SFX\\" + filename_utf8 + ".sfx";

				// Includes
				cmdLine += " -I\"" + simulPath + "\\Platform\\DirectX12\\HLSL;";
				cmdLine += simulPath + "\\Platform\\CrossPlatform\\SL\"";

				// Platform file
				cmdLine += " -P\"" + simulPath + "\\Platform\\DirectX12\\HLSL\\HLSL12.json\"";
				
				// Ouput file
				std::string outDir = r->GetShaderBinaryPath();
				for (unsigned int i = 0; i < outDir.size(); i++)
				{
					if (outDir[i] == '/')
					{
						outDir[i] = '\\';
					}
				}
				if (outDir[outDir.size() - 1] == '\\')
				{
					outDir.erase(outDir.size() - 1);
				}

				cmdLine += " -O\"" + outDir + "\"";
				// Intermediate folder
				cmdLine += " -M\"";				
				cmdLine +=simulPath + "\\Platform\\DirectX12\\sfx_intermediate\"";
				// Force
				if ((buildMode & crossplatform::ShaderBuildMode::ALWAYS_BUILD) != 0)
				{
					cmdLine += " -F";
				}
			}

			// Convert the command line to a wide string
			size_t newsize = cmdLine.size() + 1;
			wchar_t* wcstring = new wchar_t[newsize];
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, newsize, cmdLine.c_str(), _TRUNCATE);

			// Setup pipes to get cout/cerr
			SECURITY_ATTRIBUTES secAttrib	= {};
			secAttrib.nLength				= sizeof(SECURITY_ATTRIBUTES);
			secAttrib.bInheritHandle		= TRUE;
			secAttrib.lpSecurityDescriptor	= NULL;

			HANDLE coutWrite				= 0;
			HANDLE coutRead					= 0;
			HANDLE cerrWrite				= 0;
			HANDLE cerrRead					= 0;

			CreatePipe(&coutRead, &coutWrite, &secAttrib, 100);
			CreatePipe(&cerrRead, &cerrWrite, &secAttrib, 100);

			// Create the process
			STARTUPINFO startInfo			= {};
			startInfo.hStdOutput			= coutWrite;
			startInfo.hStdError				= cerrWrite;
			PROCESS_INFORMATION processInfo = {};
			bool success = CreateProcess
			(
				sfxPath.c_str(), wcstring,
				NULL, NULL, false, CREATE_NO_WINDOW, NULL, NULL,
				&startInfo, &processInfo
			);

			// Wait until if finishes
			if (success)
			{
				// Wait for the main handle and the output pipes
				HANDLE hWaitHandles[]	= {processInfo.hProcess, coutRead, cerrRead };
				DWORD ret				= WaitForMultipleObjects(3, hWaitHandles, TRUE, INFINITE);

				// Print the pipes
				const DWORD BUFSIZE = 4096;
				BYTE buff[BUFSIZE];
				DWORD dwBytesRead;
				DWORD dwBytesAvailable;
				while (PeekNamedPipe(coutRead, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
				{
					ReadFile(coutRead, buff, BUFSIZE - 1, &dwBytesRead, 0);
					SIMUL_COUT << std::string((char*)buff, (size_t)dwBytesRead).c_str();
				}
				while (PeekNamedPipe(cerrRead, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
				{
					ReadFile(cerrRead, buff, BUFSIZE - 1, &dwBytesRead, 0);
					SIMUL_COUT << std::string((char*)buff, (size_t)dwBytesRead).c_str();
				}
			}
			else
			{
				DWORD error = GetLastError();
				SIMUL_COUT << "Could not create the sfx process. Error:" << error << std::endl;
				return;
			}
		}
		else
		{
			SIMUL_COUT << "The env var SIMUL is not defined, skipping rebuild. \n";
			return;
		}
	}
#endif
}

void Effect::Load(crossplatform::RenderPlatform* r,const char* filename_utf8,const std::map<std::string,std::string>& defines)
{	
	EnsureEffect(r, filename_utf8);
#ifndef _XBOX_ONE
    SIMUL_COUT << "Loading effect:" << filename_utf8 << std::endl;
#endif
	crossplatform::Effect::Load(r, filename_utf8, defines);

    // Init the samplers heap:
    SAFE_DELETE(mSamplersHeap);
    auto rPlat          = (dx12::RenderPlatform*)r;
    mSamplersHeap       = new Heap;
    std::string name    = filename_utf8;
    name                += "_SamplersHeap";
    mSamplersHeap->Restore(rPlat, ResourceBindingLimits::NumSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, name.c_str());
    
    // Copy the default samplers into the heap:
    auto resLimits      = rPlat->GetResourceBindingLimits();
    auto nullSampler    = rPlat->GetNullSampler();
    for (int i = 0; i < ResourceBindingLimits::NumSamplers; i++)
    {
        auto sampler = (dx12::SamplerState*)samplerSlots[i];
        if (sampler)
        {
            rPlat->AsD3D12Device()->CopyDescriptorsSimple(1, mSamplersHeap->CpuHandle(), *sampler->AsD3D12SamplerState(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
        else
        {
            if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
            {
                rPlat->AsD3D12Device()->CopyDescriptorsSimple(1, mSamplersHeap->CpuHandle(), nullSampler, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }
        }
        mSamplersHeap->Offset();
    }
    // Make the handles point at the start of the heap, careful, as if we write to those
    // handles we will override this values
    mSamplersHeap->Reset();
}

void Effect::PostLoad()
{
	// Ensure slots are correct
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			dx12::Shader* v = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_VERTEX];
			dx12::Shader* p = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_PIXEL];
			dx12::Shader* c = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_COMPUTE];
			// Graphics pipeline
			if (v && p)
			{
				CheckShaderSlots(v, v->vertexShader12);
				CheckShaderSlots(p, p->pixelShader12);
				pass.second->SetConstantBufferSlots	(p->constantBufferSlots | v->constantBufferSlots);
				pass.second->SetTextureSlots		(p->textureSlots		| v->textureSlots);
				pass.second->SetTextureSlotsForSB	(p->textureSlotsForSB	| v->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(p->rwTextureSlots		| v->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(p->rwTextureSlotsForSB | v->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(p->samplerSlots		| v->samplerSlots);
			}
			// Compute pipeline
			if (c)
			{
				CheckShaderSlots(c, c->computeShader12);
				pass.second->SetConstantBufferSlots	(c->constantBufferSlots);
				pass.second->SetTextureSlots		(c->textureSlots);
				pass.second->SetTextureSlotsForSB	(c->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(c->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(c->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(c->samplerSlots);
			}
			dx12::EffectPass *ep=(EffectPass *)pass.second;
			ep->MakeResourceSlotMap();
		}
	}
}

dx12::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			auto pass12 = (dx12::EffectPass*)pass.second;
			pass12->InvalidateDeviceObjects();
		}
	}
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	crossplatform::EffectTechniqueGroup *g=groups[""];
	if(!g)
		return nullptr;
	if(g->techniques.find(name)!=g->techniques.end())
	{
		return g->techniques[name];
	}
	return NULL;
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	crossplatform::EffectTechniqueGroup *g=groups[""];
	if(!g)
		return nullptr;
	if(g->techniques_by_index.find(index)!=g->techniques_by_index.end())
	{
		return g->techniques_by_index[index];
	}
	return NULL;
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	crossplatform::Effect::Apply(deviceContext,effectTechnique,pass_num);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	crossplatform::Effect::Apply(deviceContext,effectTechnique,passname);
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n",this->filename.c_str()));
	apply_count--;
	crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
	cs->textureAssignmentMapValid = false;
	cs->rwTextureAssignmentMapValid = false;
	crossplatform::Effect::Apply(deviceContext, currentTechnique, currentPass);
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::Unapply(deviceContext);
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext, crossplatform::ConstantBufferBase *s)
{
	RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
	s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());
	crossplatform::Effect::SetConstantBuffer(deviceContext, s);
}

void Effect::CheckShaderSlots(dx12::Shader * shader, ID3DBlob * shaderBlob)
{
	HRESULT res = S_FALSE;
	// Load the shader reflection code
	if (!shader->mShaderReflection)
	{
		res = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_PPV_ARGS(&shader->mShaderReflection));
		SIMUL_ASSERT(res == S_OK);
	}

	// Get shader description
	D3D12_SHADER_DESC shaderDesc = {};
	res = shader->mShaderReflection->GetDesc(&shaderDesc);
	SIMUL_ASSERT(res == S_OK);

	// Reset the slot counter
	shader->textureSlots		= 0;
	shader->textureSlotsForSB	= 0;
	shader->rwTextureSlots		= 0;
	shader->rwTextureSlotsForSB = 0;
	shader->constantBufferSlots = 0;
	shader->samplerSlots		= 0;

	// Check the shader bound resources
	for (unsigned int i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC slotDesc = {};
		res = shader->mShaderReflection->GetResourceBindingDesc(i, &slotDesc);
		SIMUL_ASSERT(res == S_OK);

		D3D_SHADER_INPUT_TYPE type	= slotDesc.Type;
		UINT slot					= slotDesc.BindPoint;
		if (type == D3D_SIT_CBUFFER)
		{
			// NOTE: Local resources declared in the shader (like a matrix or a float4) will be added
			// to a global constant buffer but we shouldn't add it to our list
			// It looks like the only way to know if it is the global is by name or empty flag (0)
			if (slotDesc.uFlags)
			{
				shader->setUsesConstantBufferSlot(slot);
			}
		}
		else if (type == D3D_SIT_TBUFFER)
		{
			SIMUL_BREAK("Check this");
		}
		else if (type == D3D_SIT_TEXTURE)
		{
			shader->setUsesTextureSlot(slot);
		}
		else if (type == D3D_SIT_SAMPLER)
		{
			shader->setUsesSamplerSlot(slot);
		}
		else if (type == D3D_SIT_UAV_RWTYPED)
		{
			shader->setUsesRwTextureSlot(slot);
		}
		else if (type == D3D_SIT_STRUCTURED)
		{
			shader->setUsesTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED)
		{
			shader->setUsesRwTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_BYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWBYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_APPEND_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_CONSUME_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER)
		{
			SIMUL_BREAK("");
		}
		else
		{
			SIMUL_BREAK("Unknown slot type.");
		}
	}
}

Heap* Effect::GetEffectSamplerHeap()
{
    return mSamplersHeap;
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx12::EffectPass;
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}

void EffectPass::InvalidateDeviceObjects()
{
	// TO-DO: nice memory leaks here
	mComputePso = nullptr;
	mGraphicsPsoMap.clear();
}

void EffectPass::Apply(crossplatform::DeviceContext &deviceContext,bool asCompute)
{		
	// DX12: For each pass we'll have a PSO. PSOs hold: shader bytecode, input vertex format,
	// primitive topology type, blend state, rasterizer state, depth stencil state, msaa parameters, 
	// output buffer and root signature
	auto curRenderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
    // Check if requested override states:
    bool oveStateValid  = true;
    auto oveDepth       = curRenderPlat->GetOverrideDepthState();
    auto oveBlend       = curRenderPlat->GetOverrideBlendState();
    if (oveDepth && oveDepth != mInUseOverrideDepthState)
    {
        mInUseOverrideDepthState = oveDepth;
        oveStateValid = false;
    }
    if (oveBlend && oveBlend != mInUseOverrideBlendState)
    {
        mInUseOverrideBlendState = oveBlend;
        oveStateValid = false;
    }
    // Create the PSO:
	if ((mGraphicsPsoMap.empty() && !mComputePso) ||
		(((mGraphicsPsoMap.find(curRenderPlat->GetCurrentPixelFormat()) == mGraphicsPsoMap.end()) && !mComputePso)) ||
        !oveStateValid)
	{
        CreatePso(deviceContext);
	}

	// Set current RootSignature and PSO
	// We will set the DescriptorHeaps and RootDescriptorTablesArguments from ApplyContextState (at RenderPlatform)
	auto cmdList	= curRenderPlat->AsD3D12CommandList();
	if (mIsCompute)
	{   
		cmdList->SetPipelineState(mComputePso);
	}
	else
	{
		auto pFormat = curRenderPlat->GetCurrentPixelFormat();
		cmdList->SetPipelineState(mGraphicsPsoMap[pFormat]);
	}
}

void EffectPass::SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers, dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	int usedSlots			= 0;
	auto nullSampler		= rPlat->GetNullSampler();

	// The handles for the required samplers:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSamplers> srcHandles;
	for (int i = 0; i < numSamplerResourcerSlots; i++)
	{
		int slot	                        = samplerResourceSlots[i];
        crossplatform::SamplerState* samp   = nullptr;
        if (context.contextState.samplerStateOverrides.size() > 0 && context.contextState.samplerStateOverrides.HasValue(slot))
        {
            samp = context.contextState.samplerStateOverrides[slot];
            // We dont override this slot, just take a default sampler state:
            if (!samp)
            {
                samp = samplers[slot];
            }
        }
        else
        {
            samp = samplers[slot];
        }
#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
		if (!samp)
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". Sampler slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
		}
#endif
		srcHandles[slot] = *samp->AsD3D12SamplerState();
		usedSlots		|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSamplers; s++)
	{
		if (!usesSamplerSlot(s))
		{
			// Hardware tier 1 requires all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), nullSampler, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
		samplerHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	// Check slots
	unsigned int requiredSlots = GetSamplerSlots();
	if (requiredSlots != usedSlots)
	{
		unsigned int missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSamplers; i++)
		{
			unsigned int testSlot = 1 << i;
			if (testSlot & missingSlots)
			{
				SIMUL_CERR << "Resource binding error at: " << mTechName << ". Sampler slot " << i << " was not set!" << std::endl;
			}
		}
	}
#endif 
}

void EffectPass::SetConstantBuffers(crossplatform::ConstantBufferAssignmentMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	int usedSlots			= 0;
	auto nullCbv			= rPlat->GetNullCBV();

	// Clean up the handles array
	memset(mCbSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * mCbSrcHandles.size());

	// The handles for the required constant buffers:
	for (int i = 0; i < numConstantBufferResourceSlots; i++)
	{
		int slot = constantBufferResourceSlots[i];
		auto cb	 = cBuffers[slot];
		if (!cb || !usesConstantBufferSlot(slot) || slot != cb->GetIndex())
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". Constant buffer slot " << slot << " is invalid." << std::endl;
            mCbSrcHandles[slot] = nullCbv;
            continue;
		}
		auto d12cb			= (dx12::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		mCbSrcHandles[slot]	= d12cb->AsD3D12ConstantBuffer();
		usedSlots			|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumCBV; s++)
	{
		if (!usesConstantBufferSlot(s))
		{
			// Hardware tiers 1 & 2 require all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mCbSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	// Check slots
	unsigned int requiredSlots = GetConstantBufferSlots();
	if (requiredSlots != usedSlots)
	{
		unsigned int missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < ResourceBindingLimits::NumCBV; i++)
		{
			unsigned int testSlot = 1 << i;
			if (testSlot & missingSlots)
			{
				SIMUL_CERR << "Resource binding error at: " << mTechName << ". Constant buffer slot " << i << " was not set!" << std::endl;
			}
		}
	}
#endif 
}

void EffectPass::SetSRVs(crossplatform::TextureAssignmentMap& textures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullSrv			= rPlat->GetNullSRV();
	int usedSBSlots			= 0;
	int usedTextureSlots	= 0;

	// The handles for the required SRVs:
	memset(mSrvSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumSRV);
	memset(mSrvUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumSRV);

	// Iterate over the textures:
	for (int i = 0; i < numResourceSlots; i++)
	{
		int slot	= resourceSlots[i];
		auto ta		= textures[slot];

		if (!usesTextureSlot(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". Texture slot " << slot << " is invalid." << std::endl;
            mSrvSrcHandles[slot] = nullSrv;
            continue;
		}

		// If the texture is null or invalid, set a dummy:
		if (!ta.texture || !ta.texture->IsValid())
		{
			if(ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
			else
			{
				ta.texture = rPlat->GetDummy2D();
			}
		}
		mSrvSrcHandles[slot]	= *ta.texture->AsD3D12ShaderResourceView(true, ta.resourceType, ta.index, ta.mip);
		mSrvUsedSlotsArray[slot]= true;
		usedTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numSbResourceSlots; i++)
	{
		int slot = sbResourceSlots[i];
		if (mSrvUsedSlotsArray[slot])
		{
			SIMUL_CERR << "The slot: " << slot << " at pass: " << mTechName << ", has already being used by a texture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb || !usesTextureSlotForSB(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". Structured buffer slot " << slot << " is invalid." << std::endl;
            mSrvSrcHandles[slot] = nullSrv;
            continue;
		}
		mSrvSrcHandles[slot]		= *sb->AsD3D12ShaderResourceView(context);
		mSrvUsedSlotsArray[slot]	= true;
		usedSBSlots					|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		if (!usesTextureSlot(s) && !usesTextureSlotForSB(s))
		{
			// Hardware tier 1 requires all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mSrvSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	// Check texture slots
	unsigned int requiredSlotsTexture = this->GetTextureSlots();
	if (requiredSlotsTexture != usedTextureSlots)
	{
		unsigned int missingSlots = requiredSlotsTexture & (~usedTextureSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSRV; i++)
		{
			if (this->usesTextureSlot(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mTechName << ". Texture slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}

	// Check structured buffer slots
	unsigned int requiredSlotsSB = GetStructuredBufferSlots();
	if (requiredSlotsSB != usedSBSlots)
	{
		unsigned int missingSlots = requiredSlotsSB & (~usedSBSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSRV; i++)
		{
			if (usesTextureSlotForSB(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mTechName << ". Structured buffer slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}
#endif
}

void EffectPass::SetUAVs(crossplatform::TextureAssignmentMap & rwTextures, crossplatform::StructuredBufferAssignmentMap & sBuffers, dx12::Heap * frameHeap, ID3D12Device * device, crossplatform::DeviceContext & context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullUav			= rPlat->GetNullUAV();
	int usedRwSBSlots		= 0;
	int usedRwTextureSlots	= 0;

	// The the handles for the required UAVs:
	memset(mUavSrcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumUAV);
	memset(mUavUsedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumUAV);

	// Iterate over the textures:
	for (int i = 0; i < numRwResourceSlots; i++)
	{
		int slot	= rwResourceSlots[i];
		auto ta		= rwTextures[slot];

		if (!usesRwTextureSlot(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". RWTexture slot " << slot << " is invalid." << std::endl;
            mUavSrcHandles[slot] = nullUav;
            continue;
		}

		// If the texture is null or invalid, set a dummy:
		if (!ta.texture || !ta.texture->IsValid())
		{
			if (ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
            else
			{
				ta.texture = rPlat->GetDummy2D();
			}
		}
		mUavSrcHandles[slot]	= *ta.texture->AsD3D12UnorderedAccessView(ta.index, ta.mip);
		mUavUsedSlotsArray[slot]= true;
		usedRwTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numRwSbResourceSlots; i++)
	{
		int slot = rwSbResourceSlots[i];
		if (mUavUsedSlotsArray[slot])
		{
			SIMUL_CERR << "The slot: " << slot << " at pass: " << mTechName << ", has already being used by a RWTexture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb || !usesRwTextureSlotForSB(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mTechName << ". RWStructured buffer slot " << slot << " is invalid." << std::endl;
            mUavSrcHandles[slot] = nullUav;
            continue;
		}
		mUavSrcHandles[slot]		= *sb->AsD3D12UnorderedAccessView(context);
		mUavUsedSlotsArray[slot]	= true;
		usedRwSBSlots			|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		if (!usesRwTextureSlot(s) && !usesRwTextureSlotForSB(s))
		{
			// Hardware tiers 1 & 2 require all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), mUavSrcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}


#if defined(SIMUL_DX12_SLOTS_CHECK) || defined(_DEBUG)
	// Check RwTexture slots
	unsigned int requiredSlotsTexture = GetRwTextureSlots();
	if (requiredSlotsTexture != usedRwTextureSlots)
	{
		unsigned int missingSlots = requiredSlotsTexture & (~usedRwTextureSlots);
		for (int i = 0; i < ResourceBindingLimits::NumUAV; i++)
		{
			if (this->usesRwTextureSlot(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mTechName << ". RwTexture slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}

	// Check RwStructured buffer slots
	unsigned int requiredSlotsSB = GetRwStructuredBufferSlots();
	if (requiredSlotsSB != usedRwSBSlots)
	{
		unsigned int missingSlots = requiredSlotsSB & (~usedRwSBSlots);
		for (int i = 0; i < ResourceBindingLimits::NumUAV; i++)
		{
			if (usesRwTextureSlotForSB(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mTechName << ". RwStructured buffer slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}
#endif
}

void EffectPass::CreatePso(crossplatform::DeviceContext& deviceContext)
{
    mTechName = deviceContext.renderPlatform->GetContextState(deviceContext)->currentTechnique->name;
    auto curRenderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;

    // Shader bytecode
    dx12::Shader *v = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
    dx12::Shader *p = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
    dx12::Shader *c = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
    if (v && p && c)
    {
        SIMUL_BREAK("A pass can't have pixel,vertex and compute shaders.")
    }

    // Init as graphics
    if (v && p)
    {
        // Build a new pso pair <pixel format, PSO>
        std::pair<DXGI_FORMAT, ID3D12PipelineState*> psoPair;
        psoPair.first = curRenderPlat->GetCurrentPixelFormat();
        mIsCompute = false;
    
        // Try to get the input layout (if none, we dont need to set it to the pso)
        D3D12_INPUT_LAYOUT_DESC* pCurInputLayout = curRenderPlat->GetCurrentInputLayout();
    
        // Load the rendering states (blending, rasterizer and depth&stencil)
        dx12::RenderState* effectBlendState = (dx12::RenderState*)(blendState);
        dx12::RenderState* effectRasterizerState = (dx12::RenderState*)(rasterizerState);
        dx12::RenderState* effectDepthStencilState = (dx12::RenderState*)(depthStencilState);
    
        // Find out the states this effect will use
        // Blend state:
        CD3DX12_BLEND_DESC defaultBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        if (effectBlendState)
        {
            defaultBlend = CD3DX12_BLEND_DESC(effectBlendState->BlendDesc);
        }
        if (mInUseOverrideBlendState)
        {
            SIMUL_COUT << "Using override blend state for: " << mTechName << std::endl;
            defaultBlend = CD3DX12_BLEND_DESC(*mInUseOverrideBlendState);
        }
    
        // Raster state:
        CD3DX12_RASTERIZER_DESC defaultRaster = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        // CHECK: I think this is needed if we are rendering to a MSAA buffer but check it
        if (curRenderPlat->MsaaEnabled())
        {
            defaultRaster.MultisampleEnable = true;
        }
        if (effectRasterizerState)
        {
            defaultRaster = CD3DX12_RASTERIZER_DESC(effectRasterizerState->RasterDesc);
        }
        
        // Depth state:
        CD3DX12_DEPTH_STENCIL_DESC defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        if (deviceContext.viewStruct.frustum.reverseDepth)
        {
            defaultDepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        }
        if (effectDepthStencilState)
        {
            defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(effectDepthStencilState->DepthStencilDesc);
            if (deviceContext.viewStruct.frustum.reverseDepth)
            {
                defaultDepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            }
        }
        if (mInUseOverrideDepthState)
        {
            SIMUL_COUT << "Using override depth state for: " << mTechName << std::endl;
            defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(*mInUseOverrideDepthState);
        }

        // Find the current primitive topology type
        D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = {};
        D3D12_PRIMITIVE_TOPOLOGY curTopology = curRenderPlat->GetCurrentPrimitiveTopology();
    
        // Invalid
        if (curTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
        {
            primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }
        // Point
        else if (curTopology == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
        {
            primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        }
        // Lines
        else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_LINELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_LINESTRIP)
        {
            primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        }
        // Triangles
        else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
        {
            primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
        // Patches
        else
        {
            primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        }
        
        // Build the PSO description 
        D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc = {};
        gpsoDesc.pRootSignature                     = curRenderPlat->GetGraphicsRootSignature();
        gpsoDesc.VS                                 = CD3DX12_SHADER_BYTECODE(v->vertexShader12);
        gpsoDesc.PS                                 = CD3DX12_SHADER_BYTECODE(p->pixelShader12);
        gpsoDesc.InputLayout.NumElements            = pCurInputLayout ? pCurInputLayout->NumElements : 0;
        gpsoDesc.InputLayout.pInputElementDescs     = pCurInputLayout ? pCurInputLayout->pInputElementDescs : nullptr;
        gpsoDesc.RasterizerState                    = defaultRaster;
        gpsoDesc.BlendState                         = defaultBlend;
        gpsoDesc.DepthStencilState                  = defaultDepthStencil;
        gpsoDesc.SampleMask                         = UINT_MAX;
        gpsoDesc.PrimitiveTopologyType              = primitiveType;
        gpsoDesc.NumRenderTargets                   = 1;
        gpsoDesc.RTVFormats[0]                      = psoPair.first;
        gpsoDesc.DSVFormat                          = DXGI_FORMAT_D32_FLOAT;
        gpsoDesc.SampleDesc                         = curRenderPlat->GetMsaaInfo();
    
        HRESULT res = curRenderPlat->AsD3D12Device()->CreateGraphicsPipelineState
        (
            &gpsoDesc,
    #ifdef _XBOX_ONE
            IID_GRAPHICS_PPV_ARGS(&psoPair.second)
    #else
            IID_PPV_ARGS(&psoPair.second)
    #endif
        );
        SIMUL_ASSERT(res == S_OK);    
    
        std::wstring name       = L"GraphicsPSO_";
        name += std::wstring(mTechName.begin(), mTechName.end());
        if (psoPair.second)
        {
            psoPair.second->SetName(name.c_str());
        }
        else
        {
            SIMUL_ASSERT(res == S_OK);
        }
        mGraphicsPsoMap.insert(psoPair);
    }
    // Init as compute
    else if (c)
    {
        mIsCompute = true;
    
        D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc = {};
        cpsoDesc.CS = CD3DX12_SHADER_BYTECODE(c->computeShader12);
#ifndef _XBOX_ONE
        cpsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif
        cpsoDesc.pRootSignature = curRenderPlat->GetGraphicsRootSignature();
        cpsoDesc.NodeMask = 0;
        HRESULT res = curRenderPlat->AsD3D12Device()->CreateComputePipelineState
        (
            &cpsoDesc,
#ifdef _XBOX_ONE
            IID_GRAPHICS_PPV_ARGS(&mComputePso)
#else
            IID_PPV_ARGS(&mComputePso)
#endif
        );
        SIMUL_ASSERT(res == S_OK);
        if (res == S_OK)
        {
            std::wstring name       = L"ComputePSO_";
            name += std::wstring(mTechName.begin(), mTechName.end());
            mComputePso->SetName(name.c_str());
        }
        else
        {
            SIMUL_BREAK("Failed to create PSO")
            {
                return;
            }
        }
    }
    else
    {
        SIMUL_BREAK("Could't find any shaders")
    }
}

EffectPass::EffectPass():
    mInUseOverrideDepthState(nullptr),
    mInUseOverrideBlendState(nullptr)
{
}

EffectPass::~EffectPass()
{
	for (auto& ele : mGraphicsPsoMap)
	{
		SAFE_RELEASE(ele.second);
	}
	mGraphicsPsoMap.clear();
}