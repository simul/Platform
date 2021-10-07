#pragma once

#include "Export.h"
#include "Platform/Core/MemoryInterface.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/VideoBuffer.h"

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
			InvalidCodec,
			InvalidDecodeArgumentType,
			UnsupportedFeatures,
			UnsupportedDimensions
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
			PixelFormat inputFormat = PixelFormat::NV12;
			PixelFormat outputFormat = PixelFormat::RGBA_8_UNORM;
			uint32_t width = 1920;
			uint32_t height = 1080;
			uint32_t minWidth = 1200;
			uint32_t minHeight = 720;
			uint32_t maxDecodePictureBufferCount = 20;
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
			VideoDecoderResult Initialize(simul::crossplatform::RenderPlatform* renderPlatform, const VideoDecoderParams& decoderParams);
			virtual VideoDecoderResult RegisterSurface(void* surface);
			VideoDecoderResult Decode(const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0);
			virtual VideoDecoderResult UnregisterSurface();
			virtual VideoDecoderResult Shutdown();

		protected:
			virtual VideoDecoderResult Init() = 0;
			virtual VideoDecoderResult DecodeFrame(const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0) = 0;
			virtual VideoBuffer* CreateVideoBuffer() = 0;
			virtual Texture* CreateVideoTexture() = 0;
			RenderPlatform* mRenderPlatform;
			VideoDecoderParams mDecoderParams;
			void* mSurface;
			VideoBuffer* mInputBuffer;
			static constexpr uint32_t mMaxReferenceFrames = 6;
			static constexpr uint32_t mNumTextures = mMaxReferenceFrames + 1;
			Texture* mTextures[mNumTextures];
			uint32_t mNumReferenceFrames;
			uint32_t mCurrentTextureIndex;
		};
	}

}