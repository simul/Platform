#include "Platform/Vulkan/VideoDecoder.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Texture.h"
#include <algorithm>

using namespace platform;
using namespace vulkan;

VideoDecoder::VideoDecoder()
{

}

VideoDecoder::~VideoDecoder()
{

}

crossplatform::VideoDecoderResult VideoDecoder::Init()
{
	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoDecoderResult VideoDecoder::DecodeFrame(crossplatform::Texture* outputTexture, const void* buffer, size_t bufferSize, const crossplatform::VideoDecodeArgument* decodeArgs, uint32_t decodeArgCount)
{
	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoDecoderResult VideoDecoder::Shutdown()
{
	return crossplatform::VideoDecoder::Shutdown();
}

void VideoDecoder::Signal(void* context, crossplatform::Fence* fence)
{
}

void VideoDecoder::WaitOnFence(void* context, crossplatform::Fence* fence)
{
}

void VideoDecoder::WaitOnGPU()
{
}

crossplatform::VideoDecoderResult VideoDecoder::CreateVideoDevice()
{
	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoDecoderResult VideoDecoder::CreateVideoDecoder()
{

	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoDecoderResult VideoDecoder::CreateQueryObjects()
{
	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoDecoderResult VideoDecoder::CreateCommandObjects()
{
	return crossplatform::VideoDecoderResult::Ok;
}

crossplatform::VideoBuffer* VideoDecoder::CreateVideoBuffer() const
{
	return nullptr;
}

crossplatform::Texture* VideoDecoder::CreateDecoderTexture() const
{
	return new DecoderTexture();
}


void* VideoDecoder::GetGraphicsContext() const
{
	return nullptr;
}

void* VideoDecoder::GetDecodeContext() const
{
	return nullptr;
}


