#pragma once
#include "Export.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/VideoDecoder.h"
#include "Platform/Vulkan/Texture.h"
//#include "Platform/Vulkan/CommandListController.h"
#include <unordered_map>


//typedef ID3D12VideoDevice2 ID3D12VideoDeviceType;
//typedef ID3D12VideoDecodeCommandList2 ID3D12VideoDecodeCommandListType;


namespace platform
{
	namespace vulkan
	{
		class DecoderTexture : public Texture
		{
		public:
			//void ChangeState(ID3D12VideoDecodeCommandList* commandList, bool write = false);
			bool usedAsReference = false;
		};

		//! A class to implement common video encodng/decoding functionality for DirectX 12.
		class SIMUL_VULKAN_EXPORT VideoDecoder final: public crossplatform::VideoDecoder
		{
		public:
			VideoDecoder();
			~VideoDecoder();
			crossplatform::VideoDecoderResult Shutdown() override;
			//static crossplatform::VideoDecoderResult CheckSupport(ID3D12VideoDeviceType* deviceHandle, const crossplatform::VideoDecoderParams& decoderParams);

			//
			// Map of possible input and output VP formats
			// 
			//static const std::unordered_map<vk::Format, DXGI_COLOR_SPACE_TYPE> VideoFormats;

		protected:
			crossplatform::VideoDecoderResult Init() override;
			crossplatform::VideoDecoderResult DecodeFrame(crossplatform::Texture* outputTexture, const void* buffer, size_t bufferSize, const crossplatform::VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0) override;
			void* GetGraphicsContext() const override;
			void* GetDecodeContext() const override;
			crossplatform::VideoBuffer* CreateVideoBuffer() const override;
			crossplatform::Texture* CreateDecoderTexture() const override;

		private:
			void Signal(void* context, crossplatform::Fence* fence) override;
			void WaitOnFence(void* context, crossplatform::Fence* fence) override;
			void WaitOnGPU();
			crossplatform::VideoDecoderResult CreateVideoDevice();
			crossplatform::VideoDecoderResult CreateVideoDecoder();
			crossplatform::VideoDecoderResult CreateQueryObjects();
			crossplatform::VideoDecoderResult CreateCommandObjects();
			#if 0
			//! D3D12 video device for hardware accelerated video encoding/decoding
			ID3D12VideoDeviceType* mVideoDevice;
			ID3D12VideoDecoder1* mDecoder;
			ID3D12VideoDecoderHeap1* mHeap;
			ID3D12QueryHeap* mQueryHeap;
			ID3D12Resource* mQueryBuffer;
			HANDLE mSyncEvent;
			CommandListController mGraphicsCLC;
			CommandListController mDecodeCLC;
			std::vector<ID3D12Resource*> mRefTextures;
			std::vector<UINT> mRefSubresources;
			std::vector<ID3D12VideoDecoderHeap*> mRefHeaps;

			static constexpr uint32_t mMaxInputArgs = 10;
			#endif
		};
	}
}