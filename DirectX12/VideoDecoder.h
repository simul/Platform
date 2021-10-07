#pragma once

#include "Export.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/VideoDecoder.h"
#include "SimulDirectXHeader.h"

struct ID3D12VideoDevice2;
struct ID3D12VideoDecoder1;
struct ID3D12VideoDecoderHeap1;
struct ID3D12VideoProcessor1;
struct ID3D12VideoDecodeCommandList2;

typedef ID3D12VideoDevice2 ID3D12VideoDeviceType;
typedef ID3D12VideoDecodeCommandList2 ID3D12VideoDecodeCommandListType;

namespace cp = simul::crossplatform;

namespace simul
{
	namespace dx12
	{
		//! A class to implement common video encodng/decoding functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT VideoDecoder: public cp::VideoDecoder
		{
		public:
			VideoDecoder();
			~VideoDecoder();
			cp::VideoDecoderResult RegisterSurface(void* surface) override;
			
			cp::VideoDecoderResult UnregisterSurface() override;
			cp::VideoDecoderResult Shutdown() override;

			static cp::VideoDecoderResult CheckSupport(ID3D12VideoDeviceType* deviceHandle, const cp::VideoDecoderParams& decoderParams);

		protected:
			cp::VideoDecoderResult Init() override;
			cp::VideoDecoderResult DecodeFrame(const void* buffer, size_t bufferSize, const cp::VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0) override;
			cp::Texture* CreateVideoTexture() override;
			cp::VideoBuffer* CreateVideoBuffer() override;

		private:
			cp::VideoDecoderResult CreateVideoDevice();
			cp::VideoDecoderResult CreateVideoDecoder();
			cp::VideoDecoderResult CreateCommandObjects();

			void ResetCommandList();
			void ExecuteCommandList();
			void CloseCommandList();

			//! D3D12 video device for hardware accelerated video encoding/decoding
			ID3D12VideoDeviceType* mVideoDevice;
			ID3D12VideoDecoder1* mDecoder;
			ID3D12VideoDecoderHeap1* mHeap;
			ID3D12CommandQueue* mDecodeCommandQueue;
			ID3D12CommandAllocator* mDecodeCommandAllocator;
			ID3D12VideoDecodeCommandListType* mDecodeCommandList;
			
			bool mDecodeCommandListRecording;
		};
	}
}