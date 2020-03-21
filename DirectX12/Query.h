#pragma once
#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Query.h"
#include "DirectXHeader.h"

#include "Heap.h"

#include <string>
#include <map>
#include <array>

#pragma warning(disable:4251)

namespace simul
{
	namespace crossplatform
	{
		struct TextureAssignment;
	}
	namespace dx12
	{
		//! DirectX12 Query implementation
		struct SIMUL_DIRECTX12_EXPORT Query:public crossplatform::Query
		{
					Query(crossplatform::QueryType t);
			virtual ~Query() override;
			void	RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void	InvalidateDeviceObjects() override;
			void	Begin(crossplatform::DeviceContext &deviceContext) override;
			void	End(crossplatform::DeviceContext &deviceContext) override;
			bool	GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void	SetName(const char *n) override;

		protected:
			//! Holds the queries
			//ID3D12QueryHeap*		mQueryHeap;
			ID3D12QueryHeap*		heapPtr[QueryLatency];
			int						offset[QueryLatency];
			//! We cache the query type
			D3D12_QUERY_TYPE		mD3DType;
			bool mIsDisjoint;
			//! Readback buffer to read data from the query
			//ID3D12Resource*			mReadBuffer;
			//! We hold a pointer to the mapped data
			unsigned				mQueryData;
			UINT64					mTime;
		};

	}
}
