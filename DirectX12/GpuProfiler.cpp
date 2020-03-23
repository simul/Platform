#define NOMINMAX
#include "GpuProfiler.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/Query.h"
#include <stdint.h>
#include <algorithm>

using namespace simul;
using namespace dx12;

using std::string;
using std::map;

TimestampQueryManager::TimestampQueryManager()
{
	for(int i=0;i<5;i++)
	{
		mTimestampQueryHeap[i]=nullptr;
		mTimestampQueryReadBuffer[i]=nullptr;
		mTimestampQueryHeapSize[i]=0;
		mTimestampQueryHeapStart[i]=0;
		bMapped[i]=false;
	}
}
TimestampQueryManager::~TimestampQueryManager()
{

}

void TimestampQueryManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r) 
{
	renderPlatform=r;
}

void TimestampQueryManager::InvalidateDeviceObjects()
{
	for(int i=0;i<5;i++)
	{
		SAFE_RELEASE_LATER(mTimestampQueryHeap[i]);
		SAFE_RELEASE_LATER(mTimestampQueryReadBuffer[i]);
		mTimestampQueryHeap[i]=nullptr;
		mTimestampQueryReadBuffer[i]=nullptr;
		mTimestampQueryHeapSize[i]=0;
		mTimestampQueryHeapStart[i]=0;
		bMapped[i]=false;
	}
}

void TimestampQueryManager::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	if(bMapped[mTimestampQueryCurrFrame])
	{
		mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &CD3DX12_RANGE(0, 0));
		//SIMUL_COUT<<"UnMapped 0x"<<std::hex<<(unsigned long long)mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]<<std::endl;
	}
	if(mTimestampQueryData)
	{
		mTimestampQueryData=nullptr;
	}

	bMapped[mTimestampQueryCurrFrame]=false;
	mTimestampQueryCurrFrame=(mTimestampQueryCurrFrame+1)%4;
	mTimestampQueryHeapOffset=0;
	
	last_frame_number=deviceContext.frame_number;
}

void TimestampQueryManager::EndFrame(crossplatform::DeviceContext &deviceContext)
{
}

void TimestampQueryManager::GetTimestampQueryHeap(crossplatform::DeviceContext &deviceContext,ID3D12QueryHeap** heap,int *offset)
{
/*	if(deviceContext.frame_number!=last_frame_number)
	{
		StartFrame(deviceContext);
	}*/
		if(mTimestampQueryHeapOffset>=mTimestampQueryHeapSize[mTimestampQueryCurrFrame])
	{
		if(mTimestampQueryData)
		{
			mTimestampQueryData=nullptr;
		}
		if(bMapped[mTimestampQueryCurrFrame])
		{
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &CD3DX12_RANGE(0, 0));
			//SIMUL_COUT<<"UnMapped 0x"<<std::hex<<(unsigned long long)mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]<<std::endl;
		}
		bMapped[mTimestampQueryCurrFrame]=false;

		*heap=nullptr;
		*offset=0;
		mTimestampQueryHeapSize[mTimestampQueryCurrFrame]=std::max(mTimestampQueryHeapSize[mTimestampQueryCurrFrame],100)*5;
		SAFE_RELEASE_LATER(mTimestampQueryHeap[mTimestampQueryCurrFrame]);
		// Create a query heap
		HRESULT res					= S_FALSE;
		D3D12_QUERY_HEAP_DESC hDesc = {};
		hDesc.Count					= mTimestampQueryHeapSize[mTimestampQueryCurrFrame];
		hDesc.NodeMask				= 0;
		hDesc.Type					= dx12::RenderPlatform::ToD3D12QueryHeapType(crossplatform::QueryType::QUERY_TIMESTAMP);
		res							= renderPlatform->AsD3D12Device()->CreateQueryHeap(&hDesc, SIMUL_PPV_ARGS(&mTimestampQueryHeap[mTimestampQueryCurrFrame]));
		SIMUL_ASSERT(res == S_OK);

		SAFE_RELEASE_LATER(mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]);
		// Create a readback buffer to get data
		size_t sz = mTimestampQueryHeapSize[mTimestampQueryCurrFrame] * sizeof(UINT64);
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(sz),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			SIMUL_PPV_ARGS(&mTimestampQueryReadBuffer[mTimestampQueryCurrFrame])
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mTimestampQueryReadBuffer, sz)
		mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->SetName(L"mTimestampQueryReadBuffer");

		mTimestampQueryHeapOffset=0;
	}
	*offset=mTimestampQueryHeapOffset;
	*heap=mTimestampQueryHeap[mTimestampQueryCurrFrame];
	mTimestampQueryHeapOffset++;
}

unsigned long long TimestampQueryManager::GetTimestampQueryData(crossplatform::DeviceContext& deviceContext,int offset)
{
	ID3D12GraphicsCommandList* commandList=deviceContext.asD3D12Context();
	if(!mTimestampQueryData)
	{
		if(bMapped[mTimestampQueryCurrFrame])
		{
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &CD3DX12_RANGE(0, 0));
			//SIMUL_COUT<<"UnMapped 0x"<<std::hex<<(unsigned long long)mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]<<std::endl;
		}
	// At frame end, do all the resolves.
		commandList->ResolveQueryData
		(
			mTimestampQueryHeap[mTimestampQueryCurrFrame], D3D12_QUERY_TYPE_TIMESTAMP,
			0,mTimestampQueryHeapOffset,
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame], 0
		);
		int lastFrame=(mTimestampQueryCurrFrame)%4;
		if(mTimestampQueryReadBuffer[lastFrame])
		{
			HRESULT res				=mTimestampQueryReadBuffer[lastFrame]->Map(0, &CD3DX12_RANGE(0, mTimestampQueryHeapOffset), reinterpret_cast<void**>(&mTimestampQueryData));
			if(res != S_OK)
				return 0;
			bMapped[lastFrame]=true;
			//SIMUL_COUT<<"Mapped 0x"<<std::hex<<(unsigned long long)mTimestampQueryReadBuffer[lastFrame]<<std::endl;
			if(mTimestampQueryData)
				return mTimestampQueryData[offset];
		}
	}
	if(mTimestampQueryData)
	{
		return mTimestampQueryData[offset];
	}
}

void GpuProfiler::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	crossplatform::GpuProfiler::RestoreDeviceObjects(r);
	timestampQueryManager.RestoreDeviceObjects(r);
}

void GpuProfiler::InvalidateDeviceObjects() 
{
	timestampQueryManager.InvalidateDeviceObjects();
	crossplatform::GpuProfiler::InvalidateDeviceObjects();
}

void	GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::GpuProfiler::StartFrame(deviceContext);
	timestampQueryManager.StartFrame(deviceContext);
}

void GpuProfiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
	timestampQueryManager.EndFrame(deviceContext);
	crossplatform::GpuProfiler::EndFrame(deviceContext);
}

void GpuProfiler::InitQuery(crossplatform::Query *q)
{
	((dx12::Query*)q)->SetTimestampQueryManager(&timestampQueryManager);
}
