#pragma once
#if !(defined(_DURANGO) || defined(_GAMING_XBOX))
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

struct ID3D11Buffer;
struct ID3D12Resource;

namespace simul
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
			virtual void EnsureBuffer(crossplatform::RenderPlatform* r, void* context, VideoBufferType bufferType, const void* data, uint32_t dataSize) = 0;
			//! Get a pointer to the data for updating. Must call Unmap after any changes.
			virtual void* Map(void* context) = 0;
			//! Return the modified data to the device object.
			virtual void Unmap(void* context) = 0;
			//! Update the data in the buffer.
			virtual void Update(void* context, const void* data, uint32_t dataSize) = 0;

		protected:
			VideoBufferType mBufferType;
			bool mHasData;
		};
	}
}
#endif
