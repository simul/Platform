
#include "GpuProfiler.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/DirectX12/Query.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include <algorithm>
#include <stdint.h>

using namespace platform;
using namespace core;

using namespace platform;
using namespace dx12;

using std::map;
using std::string;

TimestampQueryManager::TimestampQueryManager()
{
	for (int i = 0; i < 5; i++)
	{
		mTimestampQueryHeap[i] = nullptr;
		mTimestampQueryReadBuffer[i] = nullptr;
		mTimestampQueryHeapSize[i] = 0;
		mTimestampQueryHeapStart[i] = 0;
		bMapped[i] = false;
	}
}
TimestampQueryManager::~TimestampQueryManager()
{
}

void TimestampQueryManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform = r;
}

void TimestampQueryManager::InvalidateDeviceObjects()
{
	CD3DX12_RANGE zeroRange(0, 0);
	for (int i = 0; i < 5; i++)
	{
		if (bMapped[i] && mTimestampQueryReadBuffer[i])
			mTimestampQueryReadBuffer[i]->Unmap(0, &zeroRange);
		SAFE_RELEASE_LATER(mTimestampQueryHeap[i]);
		SAFE_RELEASE_ALLOCATIR_LATER(mTimestampQueryReadBuffer[i], &mTimestampQueryReadBufferAllocations[i]);
		mTimestampQueryHeap[i] = nullptr;
		mTimestampQueryReadBuffer[i] = nullptr;
		mTimestampQueryHeapSize[i] = 0;
		mTimestampQueryHeapStart[i] = 0;
		bMapped[i] = false;
	}
}

void TimestampQueryManager::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	CD3DX12_RANGE zeroRange(0, 0);
	if (bMapped[mTimestampQueryCurrFrame])
	{
		mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &zeroRange);
	}
	if (mTimestampQueryData)
	{
		mTimestampQueryData = nullptr;
	}

	bMapped[mTimestampQueryCurrFrame] = false;
	mTimestampQueryCurrFrame = (mTimestampQueryCurrFrame + 1) % 4;
	mTimestampQueryHeapOffset = 0;

	last_frame_number = renderPlatform->GetFrameNumber();
}

void TimestampQueryManager::EndFrame(crossplatform::DeviceContext &deviceContext)
{
}

void TimestampQueryManager::GetTimestampQueryHeap(crossplatform::DeviceContext &deviceContext, ID3D12QueryHeap **heap, int *offset)
{
	if (mTimestampQueryHeapOffset >= mTimestampQueryHeapSize[mTimestampQueryCurrFrame])
	{
		if (mTimestampQueryData)
		{
			mTimestampQueryData = nullptr;
		}

		CD3DX12_RANGE zeroRange(0, 0);
		if (bMapped[mTimestampQueryCurrFrame])
		{
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &zeroRange);
		}
		bMapped[mTimestampQueryCurrFrame] = false;

		*heap = nullptr;
		*offset = 0;
		mTimestampQueryHeapSize[mTimestampQueryCurrFrame] = std::max(mTimestampQueryHeapSize[mTimestampQueryCurrFrame], 100) * 5;

		// Create a query heap
		SAFE_RELEASE_LATER(mTimestampQueryHeap[mTimestampQueryCurrFrame]);

		HRESULT res = S_FALSE;
		D3D12_QUERY_HEAP_DESC hDesc = {};
		hDesc.Count = mTimestampQueryHeapSize[mTimestampQueryCurrFrame];
		hDesc.NodeMask = 0;
		hDesc.Type = dx12::RenderPlatform::ToD3D12QueryHeapType(crossplatform::QueryType::QUERY_TIMESTAMP);
		res = renderPlatform->AsD3D12Device()->CreateQueryHeap(&hDesc, SIMUL_PPV_ARGS(&mTimestampQueryHeap[mTimestampQueryCurrFrame]));
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mTimestampQueryHeap[mTimestampQueryCurrFrame], 100)

		// Create a readback buffer to get data
		SIMUL_GPU_UNTRACK_MEMORY(mTimestampQueryReadBuffer[mTimestampQueryCurrFrame])
		SAFE_RELEASE_ALLOCATIR_LATER(mTimestampQueryReadBuffer[mTimestampQueryCurrFrame], &mTimestampQueryReadBufferAllocations[mTimestampQueryCurrFrame]);

		size_t sz = mTimestampQueryHeapSize[mTimestampQueryCurrFrame] * sizeof(UINT64);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sz);
		std::string name = "mTimestampQueryReadBuffer[" + std::to_string(mTimestampQueryCurrFrame) + "]";
		((dx12::RenderPlatform *)renderPlatform)->CreateResource(
			bufferDesc, 
			D3D12_RESOURCE_STATE_COPY_DEST, 
			nullptr, 
			D3D12_HEAP_TYPE_READBACK, 
			&mTimestampQueryReadBuffer[mTimestampQueryCurrFrame], 
			mTimestampQueryReadBufferAllocations[mTimestampQueryCurrFrame], 
			name.c_str());
		SIMUL_GPU_TRACK_MEMORY(mTimestampQueryReadBuffer[mTimestampQueryCurrFrame], sz)

		mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->SetName(StringToWString(name).c_str());
		mTimestampQueryHeapOffset = 0;
	}
	*offset = mTimestampQueryHeapOffset;
	*heap = mTimestampQueryHeap[mTimestampQueryCurrFrame];
	mTimestampQueryHeapOffset++;
}

uint64_t TimestampQueryManager::GetTimestampQueryData(crossplatform::DeviceContext &deviceContext, int offset)
{
	ID3D12GraphicsCommandList *commandList = deviceContext.asD3D12Context();
	if (!mTimestampQueryData)
	{
		if (bMapped[mTimestampQueryCurrFrame])
		{
			CD3DX12_RANGE zeroRange(0, 0);
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &zeroRange);
		}

		// At frame end, do all the resolves.
		commandList->ResolveQueryData(
			mTimestampQueryHeap[mTimestampQueryCurrFrame], D3D12_QUERY_TYPE_TIMESTAMP,
			0, mTimestampQueryHeapOffset,
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame], 0);

		int lastFrame = (mTimestampQueryCurrFrame) % 4;
		if (mTimestampQueryReadBuffer[lastFrame])
		{
			auto offsetRange = CD3DX12_RANGE(0, mTimestampQueryHeapOffset);
			HRESULT res = mTimestampQueryReadBuffer[lastFrame]->Map(0, &offsetRange, reinterpret_cast<void **>(&mTimestampQueryData));

			if (res != S_OK)
				return 0;

			bMapped[lastFrame] = true;
			uint64_t result = mTimestampQueryData[offset];
			CD3DX12_RANGE zeroRange(0, 0);
			mTimestampQueryReadBuffer[mTimestampQueryCurrFrame]->Unmap(0, &zeroRange);
			bMapped[lastFrame] = false;
			if (mTimestampQueryData)
				return result;
		}
	}
	if (mTimestampQueryData)
	{
		return mTimestampQueryData[offset];
	}
	return 0;
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

void GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
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
	((dx12::Query *)q)->SetTimestampQueryManager(&timestampQueryManager);
}
