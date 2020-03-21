/*
	GpuProfiler.h
*/

#pragma once

#include "Platform/Core/Timer.h"
#include "Platform/Core/MemoryInterface.h"
#include "Platform/Core/ProfilingInterface.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "DirectXHeader.h"

#include <string>
#include <map>

namespace simul
{
	namespace dx12
	{
		class TimestampQueryManager
		{
		public:
								TimestampQueryManager();
								~TimestampQueryManager();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) ;
			void InvalidateDeviceObjects() ;

			void				StartFrame(crossplatform::DeviceContext &deviceContext);
			void				EndFrame(crossplatform::DeviceContext &deviceContext);
			
			void				GetTimestampQueryHeap(crossplatform::DeviceContext &deviceContext,ID3D12QueryHeap** heap,int *offset);
			unsigned long long		GetTimestampQueryData(crossplatform::DeviceContext& deviceContext,int offset);
		private:
			crossplatform::RenderPlatform *renderPlatform=nullptr;
			ID3D12QueryHeap*			mTimestampQueryHeap[5];
			int							mTimestampQueryHeapSize[5];
			int							mTimestampQueryHeapStart[5];
			ID3D12Resource*				mTimestampQueryReadBuffer[5];
			bool bMapped[5];
			int							mTimestampQueryMaxHeapSize=0;
			int							mTimestampQueryHeapOffset=0;
			int							mTimestampQueryCurrFrame=0;

			//! Readback buffer to read data from the query
			unsigned long long*					mTimestampQueryData=nullptr;
			unsigned long long last_frame_number=0;
		};
	}
}