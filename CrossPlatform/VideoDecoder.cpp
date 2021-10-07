#include "VideoDecoder.h"
#include "Platform/Core/RuntimeError.h"

using namespace simul;
using namespace crossplatform;

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)          { if(p) { delete p; p=nullptr;} }
#endif

VideoDecoder::VideoDecoder()
	: mRenderPlatform(nullptr)
	, mInputBuffer(nullptr)
	, mSurface(nullptr)
	, mNumReferenceFrames(0)
	, mCurrentTextureIndex(0)
{

}

VideoDecoder::~VideoDecoder()
{

}

VideoDecoderResult VideoDecoder::Initialize(simul::crossplatform::RenderPlatform* renderPlatform, const VideoDecoderParams& decoderParams)
{
	mRenderPlatform = renderPlatform;
	mDecoderParams = decoderParams;

	if (DEC_FAILED(Init()))
	{
		Shutdown();
	}

	mInputBuffer = CreateVideoBuffer();
	mInputBuffer->EnsureBuffer(mRenderPlatform, VideoBufferType::DECODE_READ, nullptr, 0);

	for (int i = 0; i < mNumTextures; ++i)
	{
		mTextures[i] = CreateVideoTexture();
		mTextures[i]->ensureVideoTexture(mRenderPlatform, mDecoderParams.width, mDecoderParams.height, mDecoderParams.inputFormat, VideoTextureType::DECODE);
	}

	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::RegisterSurface(void* surface)
{
	mSurface = surface;
	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::Decode(const void* buffer, size_t bufferSize, const VideoDecodeArgument* decodeArgs, uint32_t decodeArgCount)
{
	DecodeFrame(buffer, bufferSize, decodeArgs, decodeArgCount);
	if (mNumReferenceFrames < mMaxReferenceFrames)
	{
		mNumReferenceFrames++;
	}
	mCurrentTextureIndex = (mCurrentTextureIndex + 1) % mNumTextures;
	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::UnregisterSurface()
{
	mSurface = nullptr;
	return VideoDecoderResult::Ok;
}

VideoDecoderResult VideoDecoder::Shutdown()
{
	for (int i = 0; i < mNumTextures; ++i)
	{
		SAFE_DELETE(mTextures[i]);
	}
	mNumReferenceFrames = 0;
	mCurrentTextureIndex = 0;
	SAFE_DELETE(mInputBuffer);
	return VideoDecoderResult::Ok;
}
