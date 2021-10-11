#if !(defined(_DURANGO) || defined(_GAMING_XBOX))
#include "Platform/DirectX12/VideoDecoder.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/GpuProfiler.h"
#include "Platform/DirectX12/Heap.h"
#include "Platform/DirectX12/VideoBuffer.h"
#include <d3d12video.h>
#include <algorithm>

using namespace simul;
using namespace dx12;

VideoDecoder::VideoDecoder()
	: mVideoDevice(nullptr)
	, mDecoder(nullptr)
	, mHeap(nullptr)
{

}

VideoDecoder::~VideoDecoder()
{
	Shutdown();
}

cp::VideoDecoderResult VideoDecoder::Init()
{
	auto r = CreateVideoDevice();
	if (DEC_FAILED(r))
	{
		return r;
	}

	r = CheckSupport(mVideoDevice, mDecoderParams);
	if (DEC_FAILED(r))
	{
		return r;
	}

	r = CreateVideoDecoder();
	if (DEC_FAILED(r))
	{
		Shutdown();
		return r;
	}

	r = CreateCommandObjects();
	if (DEC_FAILED(r))
	{
		Shutdown();
		return r;
	}

	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::RegisterSurface(cp::Texture* surface)
{
	return cp::VideoDecoder::RegisterSurface(surface);
}

cp::VideoDecoderResult VideoDecoder::DecodeFrame(const void* buffer, size_t bufferSize, const cp::VideoDecodeArgument* decodeArgs, uint32_t decodeArgCount)
{
	RenderPlatform* dx12Platform = (RenderPlatform*)mRenderPlatform;
	// We use a graphics command list for updating the buffer and a decode command list 
	// for the decoidng operation.
	mGraphicsCLC.ResetCommandList();
	mInputBuffer->Update(mGraphicsCLC.GetCommandList(), buffer, bufferSize);
	mGraphicsCLC.ExecuteCommandList();
	Signal(mGraphicsCLC.GetCommandQueue(), mDecodeFence);

	mDecodeCLC.ResetCommandList();

	ID3D12VideoDecodeCommandListType* decodeCommandList = (ID3D12VideoDecodeCommandListType*)mDecodeCLC.GetCommandList();

	Texture* dx12Surface = (Texture*)mSurface;
	ID3D12Resource* surfaceResource = dx12Surface->AsD3D12Resource();
	D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS outputArgs;
	// TODO: Support non-conversion.

	D3D12_RESOURCE_STATES surfaceStateBefore = dx12Surface->GetState();
	D3D12_RESOURCE_STATES surfaceStateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
	// Change surface texture state.
	decodeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(surfaceResource, surfaceStateBefore, surfaceStateAfter));

	outputArgs.pOutputTexture2D = surfaceResource;
	outputArgs.OutputSubresource = 0;

	D3D12_VIDEO_DECODE_CONVERSION_ARGUMENTS& convArgs = outputArgs.ConversionArguments;
	if (mDecoderParams.decodeFormat != mDecoderParams.surfaceFormat)
	{
		convArgs.Enable = true;
		// TODO: Don't hardcode color spaces.
		convArgs.DecodeColorSpace = DXGI_COLOR_SPACE_TYPE::DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;
		convArgs.OutputColorSpace = DXGI_COLOR_SPACE_TYPE::DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		((DecoderTexture*)mTextures[mCurrentTextureIndex])->ChangeState(decodeCommandList, true);
		convArgs.pReferenceTexture2D = mTextures[mCurrentTextureIndex]->AsD3D12Resource();
		convArgs.ReferenceSubresource = 0;
	}
	else
	{
		convArgs.Enable = false;
	}

	D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS inputArgs;
	inputArgs.CompressedBitstream.Size = bufferSize;
	inputArgs.CompressedBitstream.Offset = 0;
	inputArgs.CompressedBitstream.pBuffer = mInputBuffer->AsD3D12Buffer();
	inputArgs.pHeap = mHeap;

	// D3D12 accepts a maximum of 10 arguments. 
	inputArgs.NumFrameArguments = std::min<uint32_t>(decodeArgCount, 10);
	for (uint32_t i = 0; i < inputArgs.NumFrameArguments; ++i)
	{
		auto& frameArgs = inputArgs.FrameArguments[i];
		frameArgs.pData = decodeArgs[i].data;
		frameArgs.Size = decodeArgs[i].size;
		switch (decodeArgs[i].type)
		{
		case cp::VideoDecodeArgumentType::PictureParameters:
			frameArgs.Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE::D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS;
			break;
		case cp::VideoDecodeArgumentType::MaxValid:
			frameArgs.Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE::D3D12_VIDEO_DECODE_ARGUMENT_TYPE_MAX_VALID;
			break;
		case cp::VideoDecodeArgumentType::SliceControl:
			frameArgs.Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE::D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL;
			break;
		case cp::VideoDecodeArgumentType::InverseQuantizationMatrix:
			frameArgs.Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE::D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX;
			break;
		default:
			SIMUL_CERR_ONCE << "Invalid video decode argument type!";
			return cp::VideoDecoderResult::InvalidDecodeArgumentType;
		}
	}

	inputArgs.ReferenceFrames.NumTexture2Ds = mNumReferenceFrames;
	if (mNumReferenceFrames)
	{
		std::vector<ID3D12Resource*> refTexs(mNumReferenceFrames);
		std::vector<ID3D12VideoDecoderHeap*> heaps;

		int index = 0;
		int i = mCurrentTextureIndex - 1;
		size_t numTextures = mTextures.size();
		while (index < mNumReferenceFrames)
		{
			((DecoderTexture*)mTextures[i])->ChangeState(decodeCommandList, false);
			refTexs[index] = mTextures[i]->AsD3D12Resource();
			heaps[index] = mHeap;
			++index;
			// Wrap back around like a circular buffer.
			i = (i - 1) % numTextures;
		}
		inputArgs.ReferenceFrames.ppTexture2Ds = refTexs.data();
		// Specifies 0 used for each texture.
		inputArgs.ReferenceFrames.pSubresources = NULL;
		inputArgs.ReferenceFrames.ppHeaps = heaps.data();
	}

	decodeCommandList->DecodeFrame(mDecoder, &outputArgs, &inputArgs);

	// Change surface texture state back.
	decodeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(surfaceResource, surfaceStateAfter, surfaceStateBefore));

	// Ensure gpu waits for the buffer to be uploaded before decoding.
	WaitOnFence(mGraphicsCLC.GetCommandQueue(), mDecodeFence);

	mDecodeCLC.ExecuteCommandList();
	
	// Ensure gpu waits for the output surface to be written to.
	Signal(mDecodeCLC.GetCommandQueue(), mDecodeFence);
	WaitOnFence(mGraphicsCLC.GetCommandQueue(), mDecodeFence);

	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::UnregisterSurface()
{
	return cp::VideoDecoder::UnregisterSurface();
}

cp::VideoDecoderResult VideoDecoder::Shutdown()
{
	cp::VideoDecoder::Shutdown();
	UnregisterSurface();
	SAFE_DELETE(mInputBuffer);
	mGraphicsCLC.Release();
	mDecodeCLC.Release();
	SAFE_RELEASE(mHeap);
	SAFE_RELEASE(mDecoder);
	SAFE_RELEASE(mVideoDevice);

	return cp::VideoDecoderResult::Ok;
}

void VideoDecoder::Signal(void* context, cp::Fence* fence)
{
	Fence* f = (Fence*)fence;
	f->value++;
	((ID3D12CommandQueue*)context)->Signal(f->AsD3D12Fence(), f->value);
}

void VideoDecoder::WaitOnFence(void* context, cp::Fence* fence)
{
	Fence* f = (Fence*)fence;
	((ID3D12CommandQueue*)context)->Wait(f->AsD3D12Fence(), f->value);
}

cp::VideoDecoderResult VideoDecoder::CreateVideoDevice()
{
	ID3D12Device* device = mRenderPlatform->AsD3D12Device();

	IDXGIFactory4* factory = nullptr;
	if (FAILED(CreateDXGIFactory2(0, SIMUL_PPV_ARGS(&factory))))
	{
		SIMUL_CERR_ONCE << "Error occurred trying to create D3D12 factory.";
		return cp::VideoDecoderResult::VideoAPIError;
	}
	LUID luid = device->GetAdapterLuid();
	IDXGIAdapter1* hardwareAdapter = nullptr;
	factory->EnumAdapterByLuid(luid, SIMUL_PPV_ARGS(&hardwareAdapter));
	if (FAILED(factory->EnumAdapterByLuid(luid, SIMUL_PPV_ARGS(&hardwareAdapter))))
	{
		SIMUL_CERR_ONCE << "Error occurred trying to get adapter.";
		SAFE_RELEASE(factory);
		return cp::VideoDecoderResult::VideoAPIError;
	}
	SAFE_RELEASE(factory);
	if (FAILED(D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_12_1, SIMUL_PPV_ARGS(&mVideoDevice))))
	{
		SIMUL_CERR_ONCE << "Error occurred trying to create video device.";
		SAFE_RELEASE(hardwareAdapter);
		return cp::VideoDecoderResult::VideoAPIError;
	}
	SAFE_RELEASE(hardwareAdapter);
	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::CreateVideoDecoder()
{
	D3D12_VIDEO_DECODER_DESC dDesc;
	dDesc.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
	dDesc.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
	if (mDecoderParams.codec == cp::VideoCodec::H264)
	{
		dDesc.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
	}
	else
	{
		dDesc.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
	}
	dDesc.NodeMask = 0;

	if (FAILED(mVideoDevice->CreateVideoDecoder1(&dDesc, nullptr, SIMUL_PPV_ARGS(&mDecoder))))
	{
		SIMUL_CERR_ONCE << "Error occurred trying to create the video decoder.";
		return cp::VideoDecoderResult::VideoAPIError;
	}

	D3D12_VIDEO_DECODER_HEAP_DESC hDesc;
	hDesc.Configuration = dDesc.Configuration;
	hDesc.DecodeWidth = mDecoderParams.width;
	hDesc.DecodeHeight = mDecoderParams.height;
	hDesc.BitRate = mDecoderParams.bitRate;
	hDesc.Format = RenderPlatform::ToDxgiFormat(mDecoderParams.decodeFormat);
	hDesc.MaxDecodePictureBufferCount = mDecoderParams.maxDecodePictureBufferCount;
	hDesc.FrameRate = DXGI_RATIONAL{ mDecoderParams.frameRate, 1 };
	hDesc.NodeMask = 0;

	if (FAILED(mVideoDevice->CreateVideoDecoderHeap1(&hDesc, nullptr, SIMUL_PPV_ARGS(&mHeap))))
	{
		SIMUL_CERR_ONCE << "Error occurred trying to create the video decoder heap.";
		return cp::VideoDecoderResult::VideoAPIError;
	}

	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::CreateCommandObjects()
{
	RenderPlatform* dx12PF = (RenderPlatform*)mRenderPlatform;
	mGraphicsCLC.Initialize(dx12PF, D3D12_COMMAND_LIST_TYPE_DIRECT, "DecoderGraphics", dx12PF->GetCommandQueue());
	mDecodeCLC.Initialize(dx12PF, D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, "Decode");
	return cp::VideoDecoderResult::Ok;
}

cp::VideoBuffer* VideoDecoder::CreateVideoBuffer() const
{
	return new VideoBuffer();
}

cp::Texture* VideoDecoder::CreateDecoderTexture() const
{
	return new DecoderTexture();
}


void* VideoDecoder::GetGraphicsContext() const
{
	return mGraphicsCLC.GetCommandList();
}

cp::VideoDecoderResult VideoDecoder::CheckSupport(ID3D12VideoDeviceType* device, const cp::VideoDecoderParams& decoderParams)
{
	D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT fd;
	fd.BitRate = decoderParams.bitRate;
	fd.DecodeFormat = RenderPlatform::ToDxgiFormat(decoderParams.decodeFormat);
	fd.NodeIndex = 0;
	fd.Width = decoderParams.width;
	fd.Height = decoderParams.height;
	fd.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
	fd.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
	fd.DecodeTier = D3D12_VIDEO_DECODE_TIER::D3D12_VIDEO_DECODE_TIER_2;

	switch (decoderParams.codec)
	{
	case cp::VideoCodec::H264:
		fd.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
		break;
	case cp::VideoCodec::HEVC:
		fd.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
		break;
	default:
		SIMUL_CERR_ONCE << "Invalid video codec provided!";
		return cp::VideoDecoderResult::InvalidCodec;
	}

	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &fd, sizeof(fd))))
	{
		SIMUL_CERR_ONCE << "Error occurred checking feature support of the video device.";
		return cp::VideoDecoderResult::VideoAPIError;
	}

	if (fd.SupportFlags != D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED)
	{
		SIMUL_CERR_ONCE << "The video device does not have all the required features for this decoding operation.";
		return cp::VideoDecoderResult::UnsupportedFeatures;
	}

	if (fd.ConfigurationFlags == D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_HEIGHT_ALIGNMENT_MULTIPLE_32_REQUIRED)
	{
		SIMUL_CERR_ONCE << "Height of output must be a multiple of 32!";
		return cp::VideoDecoderResult::UnsupportedDimensions;
	}

	return cp::VideoDecoderResult::Ok;
}

void DecoderTexture::ChangeState(ID3D12VideoDecodeCommandListType* commandList, bool write)
{
	D3D12_RESOURCE_STATES stateBefore = mResourceState;
	D3D12_RESOURCE_STATES stateAfter;

	if (write)
	{
		stateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
	}
	else
	{
		stateAfter = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
	}

	if (stateBefore == stateAfter)
	{
		return;
	}

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTextureDefault, stateBefore, stateAfter));
	mResourceState = stateAfter;
}
#endif