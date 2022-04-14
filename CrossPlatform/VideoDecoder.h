#pragma once

#include "Export.h"
#include "Platform/Core/MemoryInterface.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/VideoBuffer.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define DEC_FAILED(r) \
	(r != simul::crossplatform::VideoDecoderResult::Ok)

namespace simul
{
	/// The namespace and library for cross-platform base classes, with abstract video decoding functionality.
	namespace crossplatform
	{
		enum class VideoDecoderResult
		{
			Ok = 0,
			VideoAPIError,
			D3D12APIError,
			InvalidCodec,
			InvalidDecodeArgumentType,
			UnsupportedFeatures,
			UnsupportedDimensions,
			DecodingFailed
		};

		enum class VideoCodec
		{
			H264,
			HEVC
		};

		enum class DeinterlaceMode
		{
			None,
			Bob,
			Custom
		};

		struct VideoDecoderParams
		{
			uint32_t bitRate = 0;
			uint32_t frameRate = 60;
			VideoCodec codec = VideoCodec::HEVC;
			// Native format stream is decoded to.
			PixelFormat decodeFormat = PixelFormat::NV12;
			// Format of the output texture the native format is converted to. 
			PixelFormat outputFormat = PixelFormat::NV12;
			uint32_t width = 1920;
			uint32_t height = 1080;
			uint32_t minWidth = 1200;
			uint32_t minHeight = 720;
			uint32_t maxDecodePictureBufferCount = 16;
			DeinterlaceMode deinterlaceMode = DeinterlaceMode::None;
		};

		enum class VideoDecodeArgumentType
		{
			PictureParameters = 0,
			InverseQuantizationMatrix = 1,
			SliceControl = 2,
			MaxValid = 3
		};

		struct VideoDecodeArgument
		{
			VideoDecodeArgumentType type = VideoDecodeArgumentType::PictureParameters;
			uint32_t size = 0;
			void* data = nullptr;
		};

		class SIMUL_CROSSPLATFORM_EXPORT VideoDecoder
		{
		public:
			VideoDecoder();
			virtual ~VideoDecoder();
			VideoDecoderResult Initialize(simul::crossplatform::RenderPlatform* renderPlatform, const VideoDecoderParams& decoderParams, bool validateDecoding = false);
			VideoDecoderResult Decode(Texture* outputTexture, const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0);
			virtual VideoDecoderResult Shutdown();

		protected:
			virtual VideoDecoderResult Init() = 0;
			virtual VideoDecoderResult DecodeFrame(Texture* outputTexture, const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0) = 0;
			virtual void* GetGraphicsContext() const = 0;
			virtual void* GetDecodeContext() const = 0;
			virtual VideoBuffer* CreateVideoBuffer() const = 0;
			virtual Texture* CreateDecoderTexture() const = 0;
			virtual void Signal(void* context, Fence* fence) = 0;
			virtual void WaitOnFence(void* context, Fence* fence) = 0;
			bool IsIDR(const uint8_t* data, size_t size) const;

			RenderPlatform* mRenderPlatform;
			VideoDecoderParams mDecoderParams;
			VideoBuffer* mInputBuffer;
			std::vector<Texture*> mTextures;
			Fence* mDecodeFence;
			bool mFeaturesSupported;
			bool mValidateDecoding;
		};
	}

}