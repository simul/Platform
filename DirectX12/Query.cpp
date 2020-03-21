#define NOMINMAX

#include "Effect.h"
#include "Texture.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/Query.h"

#include <algorithm>
#include <string>

using namespace simul;
using namespace dx12;

Query::Query(crossplatform::QueryType t):
	crossplatform::Query(t),
	//mQueryHeap(nullptr),
	//mReadBuffer(nullptr),
	//mQueryData(nullptr),
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
	//mQueryHeap->SetName(std::wstring(n.begin(),n.end()).c_str());
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	InvalidateDeviceObjects();
	renderPlatform = r;
	auto renderPlatformDx12 = (dx12::RenderPlatform*)r;

	// Create a query heap
	HRESULT res					= S_FALSE;
	mD3DType					= dx12::RenderPlatform::ToD3dQueryType(type);
/*	D3D12_QUERY_HEAP_DESC hDesc = {};
	hDesc.Count					= QueryLatency;
	hDesc.NodeMask				= 0;
	hDesc.Type					= dx12::RenderPlatform::ToD3D12QueryHeapType(type);
	res							= r->AsD3D12Device()->CreateQueryHeap(&hDesc, SIMUL_PPV_ARGS(&mQueryHeap));
	SIMUL_ASSERT(res == S_OK);

	// Create a readback buffer to get data
	size_t sz = QueryLatency * sizeof(UINT64);
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sz),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
        SIMUL_PPV_ARGS(&mReadBuffer)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mReadBuffer, sz)
	mReadBuffer->SetName(L"QueryReadBuffer");*/
}

void Query::InvalidateDeviceObjects() 
{
	//SAFE_RELEASE_LATER(mQueryHeap);
	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i]= true;	// ?????????
		doneQuery[i] = false;
	}
	//SAFE_RELEASE_LATER(mReadBuffer);
	//mQueryData = nullptr;
}

void Query::Begin(crossplatform::DeviceContext &)
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
	ID3D12GraphicsCommandList* commandList=deviceContext.asD3D12Context();
	auto *rp=(dx12::RenderPlatform*)renderPlatform;
	rp->GetTimestampQueryHeap(deviceContext,&heapPtr[currFrame],&(offset[currFrame]));
	commandList->EndQuery
	(
		heapPtr[currFrame],
		mD3DType,
		offset[currFrame]
	);
}

bool Query::GetData(crossplatform::DeviceContext& deviceContext,void *data,size_t sz)
{
	gotResults[currFrame]	= true;
	int prevFrame=currFrame;
	currFrame				= (currFrame + 1) % QueryLatency;
	// We provide frequency information
	if (mIsDisjoint)
	{
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
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
	auto *rp=(dx12::RenderPlatform*)renderPlatform;
	unsigned mTime=rp->GetTimestampQueryData(deviceContext,offset[currFrame]);
	// Get the values from the buffer
	/*HRESULT res				= S_FALSE;
	res						= mReadBuffer->Map(0, &CD3DX12_RANGE(0, 1), reinterpret_cast<void**>(&mQueryData));
	SIMUL_ASSERT(res == S_OK);
	
	UINT64* d			= (UINT64*)mQueryData;
	UINT64 time			= d[offset[currFrame]];
	mTime				= time;*/
	memcpy(data, &mTime, sz);
	
	//mReadBuffer->Unmap(0, &CD3DX12_RANGE(0, 0));

	return true;
} 
