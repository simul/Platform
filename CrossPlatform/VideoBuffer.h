#pragma once

#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

struct ID3D11Buffer;
struct ID3D12Resource;

namespace platform
{
	namespace crossplatform
	{
		enum class VideoBufferType
		{
			UNKNOWN,
			ENCODE_READ,
			DECODE_READ,
			PROCESS_READ
		};

		enum class VideoBufferState
		{
			COMMON,
			UPLOAD,
			OPERATION
		};

		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT VideoBuffer
		{
		public:
			VideoBuffer();
			virtual ~VideoBuffer();
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11Buffer *AsD3D11Buffer()
			{
				return nullptr;
			}
			virtual ID3D11Buffer * const AsD3D11Buffer() const
			{
				return nullptr;
			}
			virtual ID3D12Resource * const AsD3D12Buffer() const
			{
				return nullptr;
			}
			virtual ID3D12Resource *  AsD3D12Buffer() 
			{
				return nullptr;
			}
			VideoBufferType GetBufferType()
			{
				return mBufferType;
			}
			//! Set up as a buffer for video encoding/decoding read operations.
			virtual void EnsureBuffer(crossplatform::RenderPlatform* r, VideoBufferType bufferType, uint32_t bufferSize) = 0;
			//! Change buffer state for updating or for video opertions.
			virtual void ChangeState(void* videoContext, VideoBufferState bufferState) = 0;
			//! Update the data in the buffer.
			virtual void Update(void* graphicsContext, const void* data, uint32_t dataSize) = 0;

		protected:
			VideoBufferType mBufferType;
			bool mHasData;
		};
	}
}
