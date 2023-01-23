

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
	mIsDisjoint(false),
	mTime(0)
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

void Query::SetTimestampQueryManager(TimestampQueryManager *m)
{
	timestampQueryManager=m;
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
	timestampQueryManager->GetTimestampQueryHeap(deviceContext,&heapPtr[currFrame],&(offset[currFrame]));
	commandList->EndQuery(heapPtr[currFrame],mD3DType,offset[currFrame]);
}

bool Query::GetData(crossplatform::DeviceContext& deviceContext,void *data,size_t sz)
{
	if (!data)
		return false;

	gotResults[currFrame]	= true;
	int prevFrame=currFrame;
	currFrame				= (currFrame + 1) % QueryLatency;
	// We provide frequency information
	if (mIsDisjoint)
	{
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
		crossplatform::DisjointQueryStruct* disjointData = (crossplatform::DisjointQueryStruct*)data;
		disjointData->Disjoint	= false;
		disjointData->Frequency	= rPlat->GetTimeStampFreq();
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
	*(unsigned long long*)data = (unsigned long long)timestampQueryManager->GetTimestampQueryData(deviceContext, offset[currFrame]);
	
	//mReadBuffer->Unmap(0, &CD3DX12_RANGE(0, 0));

	return true;
} 
