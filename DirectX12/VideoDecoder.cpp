#include "Platform/DirectX12/VideoDecoder.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/GpuProfiler.h"
#include "Platform/DirectX12/Heap.h"
#include "Platform/DirectX12/VideoBuffer.h"
#include "Platform/DirectX12/Texture.h"
#include <algorithm>
#include <dxva.h>

using namespace simul;
using namespace dx12;

const std::unordered_map<DXGI_FORMAT, DXGI_COLOR_SPACE_TYPE> VideoDecoder::VideoFormats
{
	{ DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
	{ DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
	{ DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
	{ DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
	{ DXGI_FORMAT_P010, DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 },
	{ DXGI_FORMAT_P016, DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 },
	{ DXGI_FORMAT_420_OPAQUE, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
	{ DXGI_FORMAT_YUY2, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
	{ DXGI_FORMAT_AYUV, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
	{ DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 }
};

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

	mFeaturesSupported = true;

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

	mRefTextures.resize(mTextures.size());
	mRefSubresources.resize(mTextures.size());
	mRefHeaps.resize(mTextures.size());

	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::DecodeFrame(cp::Texture* outputTexture, const void* buffer, size_t bufferSize, const cp::VideoDecodeArgument* decodeArgs, uint32_t decodeArgCount)
{
	if (!mFeaturesSupported)
	{
		SIMUL_CERR << "Driver does not support this decoding configuration!";
		return cp::VideoDecoderResult::UnsupportedFeatures;
	}
	RenderPlatform* dx12Platform = (RenderPlatform*)mRenderPlatform;
	ID3D12VideoDecodeCommandListType* decodeCommandList = (ID3D12VideoDecodeCommandListType*)mDecodeCLC.GetCommandList();
	ID3D12GraphicsCommandList* graphicsCommandList = (ID3D12GraphicsCommandList*)mGraphicsCLC.GetCommandList();
	Texture* dx12OutputTexture = (Texture*)outputTexture;
	ID3D12Resource* outputResource = dx12OutputTexture->AsD3D12Resource();

	mGraphicsCLC.ResetCommandList();
	mDecodeCLC.ResetCommandList();

	mInputBuffer->Update(graphicsCommandList, buffer, (uint32_t)bufferSize);
		
	mGraphicsCLC.ExecuteCommandList();
	Signal(mGraphicsCLC.GetCommandQueue(), mDecodeFence);


	uint32_t currPic = 0;

	// Input Arguments 

	D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS inputArgs;
	inputArgs.CompressedBitstream.Size = bufferSize;
	inputArgs.CompressedBitstream.Offset = 0;
	inputArgs.CompressedBitstream.pBuffer = mInputBuffer->AsD3D12Buffer();
	inputArgs.pHeap = mHeap;

	// DXVA accepts a maximum of 10 arguments. 
	inputArgs.NumFrameArguments = std::min<uint32_t>(decodeArgCount, mMaxInputArgs);
	bool picParams = false;

	for (uint32_t i = 0; i < inputArgs.NumFrameArguments; ++i)
	{
		auto& frameArgs = inputArgs.FrameArguments[i];

		if (!decodeArgs[i].data || !decodeArgs[i].size)
		{
			SIMUL_CERR << "Invalid data in frame argument!";
			return cp::VideoDecoderResult::InvalidDecodeArgumentType;
		}
		frameArgs.pData = decodeArgs[i].data;
		
		frameArgs.Size = decodeArgs[i].size;
		switch (decodeArgs[i].type)
		{
		case cp::VideoDecodeArgumentType::PictureParameters:
			frameArgs.Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE::D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS;
			if (mDecoderParams.codec == cp::VideoCodec::H264)
			{
				frameArgs.Size = sizeof(DXVA_PicParams_H264);
				DXVA_PicParams_H264* pp = static_cast<DXVA_PicParams_H264*>(frameArgs.pData);
				// TODO:: Implement
			}
			else if (mDecoderParams.codec == cp::VideoCodec::HEVC)
			{
				frameArgs.Size = sizeof(DXVA_PicParams_HEVC);
				DXVA_PicParams_HEVC* pp = static_cast<DXVA_PicParams_HEVC*>(frameArgs.pData);

				currPic = pp->CurrPic.Index7Bits;
				DecoderTexture* tex = (DecoderTexture*)mTextures[currPic];
				tex->usedAsReference = false;
				for (int i = 0; i < ARRAY_SIZE(pp->RefPicList); ++i)
				{
					uint32_t index = pp->RefPicList[i].AssociatedFlag;
					tex = (DecoderTexture*)mTextures[index];
					tex->usedAsReference = pp->RefPicList[i].bPicEntry != 0x7f && pp->RefPicList[i].bPicEntry != 0xff;
				}
			}
			picParams = true;
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
			SIMUL_CERR << "Invalid video decode argument type!";
			return cp::VideoDecoderResult::InvalidDecodeArgumentType;
		}
	}

	if (!picParams)
	{
		SIMUL_CERR << "No picture parameters in frame arguments!";
		return cp::VideoDecoderResult::InvalidDecodeArgumentType;
	}

	inputArgs.ReferenceFrames.NumTexture2Ds = mTextures.size();

	for(int i = 0; i < mTextures.size(); ++i)
	{
		DecoderTexture* tex = (DecoderTexture*)mTextures[i];
		if (tex->usedAsReference)
		{
			tex->ChangeState(decodeCommandList, false);
			mRefTextures[i] = tex->AsD3D12Resource();
		}
		else
		{
			mRefTextures[i] = nullptr;
		}
		mRefSubresources[i] = 0;
		mRefHeaps[i] = mHeap;
	}
	inputArgs.ReferenceFrames.ppTexture2Ds = mRefTextures.data();
	// Specifies 0 used for each texture.
	inputArgs.ReferenceFrames.pSubresources = mRefSubresources.data();
	inputArgs.ReferenceFrames.ppHeaps = mRefHeaps.data();
	


	// Output Arguments

	D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS outputArgs;
	outputArgs.OutputSubresource = 0;

	// This texture will  hold the native output regardless of whether conversion is enabled.
	((DecoderTexture*)mTextures[currPic])->ChangeState(decodeCommandList, true);

	D3D12_RESOURCE_STATES outputTextureStateBefore = dx12OutputTexture->GetState();
	D3D12_RESOURCE_STATES outputTextureStateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
	// Change surface texture state.
	//decodeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputResource, outputTextureStateBefore, outputTextureStateAfter, 0));

	D3D12_VIDEO_DECODE_CONVERSION_ARGUMENTS& convArgs = outputArgs.ConversionArguments;

	outputArgs.pOutputTexture2D = outputResource;
	DXGI_FORMAT dxgiDecodeFormat = RenderPlatform::ToDxgiFormat(mDecoderParams.decodeFormat);
	DXGI_FORMAT dxgiSurfaceFormat = RenderPlatform::ToDxgiFormat(mDecoderParams.outputFormat);
	// Conversion is always enabled because the output texture is expected to be in
	// same colour space as the input texture.
	convArgs.Enable = true;
	convArgs.DecodeColorSpace = VideoFormats.find(dxgiDecodeFormat)->second;
	convArgs.OutputColorSpace = VideoFormats.find(dxgiSurfaceFormat)->second;
	convArgs.pReferenceTexture2D = mTextures[currPic]->AsD3D12Resource();
	convArgs.ReferenceSubresource = 0;


	// Set to required state for decoding.
	mInputBuffer->ChangeState(decodeCommandList, false);

	decodeCommandList->DecodeFrame(mDecoder, &outputArgs, &inputArgs);

	mInputBuffer->ChangeState(decodeCommandList, true);

	// Change surface texture state back.
	//decodeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputResource, outputTextureStateAfter, outputTextureStateBefore));

	// Ensure gpu waits for the buffer to be uploaded before decoding.
	WaitOnFence(mDecodeCLC.GetCommandQueue(), mDecodeFence);

	mDecodeCLC.ExecuteCommandList();
	
	// Ensure gpu waits for the output surface to be written to.
	Signal(mDecodeCLC.GetCommandQueue(), mDecodeFence);
	WaitOnFence(mGraphicsCLC.GetCommandQueue(), mDecodeFence);

	return cp::VideoDecoderResult::Ok;
}

cp::VideoDecoderResult VideoDecoder::Shutdown()
{
	cp::VideoDecoder::Shutdown();
	SAFE_DELETE(mInputBuffer);
	mGraphicsCLC.Release();
	mDecodeCLC.Release();
	SAFE_RELEASE(mHeap);
	SAFE_RELEASE(mDecoder);
	SAFE_RELEASE(mVideoDevice);

	mRefTextures.clear();
	mRefSubresources.clear();
	mRefHeaps.clear();

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
		SIMUL_CERR << "Error occurred trying to create D3D12 factory.";
		return cp::VideoDecoderResult::VideoAPIError;
	}
	LUID luid = device->GetAdapterLuid();
	IDXGIAdapter1* hardwareAdapter = nullptr;
	factory->EnumAdapterByLuid(luid, SIMUL_PPV_ARGS(&hardwareAdapter));
	if (FAILED(factory->EnumAdapterByLuid(luid, SIMUL_PPV_ARGS(&hardwareAdapter))))
	{
		SIMUL_CERR << "Error occurred trying to get adapter.";
		SAFE_RELEASE(factory);
		return cp::VideoDecoderResult::VideoAPIError;
	}
	SAFE_RELEASE(factory);
	if (FAILED(D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_12_1, SIMUL_PPV_ARGS(&mVideoDevice))))
	{
		SIMUL_CERR << "Error occurred trying to create video device.";
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
		SIMUL_CERR << "Error occurred trying to create the video decoder.";
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
		SIMUL_CERR << "Error occurred trying to create the video decoder heap.";
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

void* VideoDecoder::GetDecodeContext() const
{
	return mDecodeCLC.GetCommandList();
}

cp::VideoDecoderResult VideoDecoder::CheckSupport(ID3D12VideoDeviceType* device, const cp::VideoDecoderParams& decoderParams)
{
	D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT ds;
	ds.BitRate = decoderParams.bitRate;
	ds.DecodeFormat = RenderPlatform::ToDxgiFormat(decoderParams.decodeFormat);
	ds.NodeIndex = 0;
	ds.Width = decoderParams.width;
	ds.Height = decoderParams.height;
	ds.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
	ds.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
	ds.DecodeTier = D3D12_VIDEO_DECODE_TIER::D3D12_VIDEO_DECODE_TIER_2;

	switch (decoderParams.codec)
	{
	case cp::VideoCodec::H264:
		ds.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
		break;
	case cp::VideoCodec::HEVC:
		ds.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
		break;
	default:
		SIMUL_CERR << "Invalid video codec provided!";
		return cp::VideoDecoderResult::InvalidCodec;
	}

	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &ds, sizeof(ds))))
	{
		SIMUL_CERR << "Error occurred checking decode feature support of the video device.";
		return cp::VideoDecoderResult::VideoAPIError;
	}

	if (ds.SupportFlags != D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED)
	{
		SIMUL_CERR << "The video device does not have all the required features for this decoding operation.";
		return cp::VideoDecoderResult::UnsupportedFeatures;
	}

	if (ds.ConfigurationFlags == D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_HEIGHT_ALIGNMENT_MULTIPLE_32_REQUIRED)
	{
		SIMUL_CERR << "Height of output must be a multiple of 32!";
		return cp::VideoDecoderResult::UnsupportedDimensions;
	}

	D3D12_FEATURE_DATA_VIDEO_DECODE_CONVERSION_SUPPORT cs;
	cs.BitRate = ds.BitRate;
	cs.FrameRate = ds.FrameRate;
	cs.NodeIndex = ds.NodeIndex;
	cs.Configuration = ds.Configuration;
	cs.DecodeSample.Width = ds.Width;
	cs.DecodeSample.Height = ds.Height;
	cs.DecodeSample.Format.Format = ds.DecodeFormat;
	cs.DecodeSample.Format.ColorSpace = VideoFormats.find(ds.DecodeFormat)->second;
	cs.OutputFormat.Format = RenderPlatform::ToDxgiFormat(decoderParams.outputFormat);
	cs.OutputFormat.ColorSpace = VideoFormats.find(cs.OutputFormat.Format)->second; 

	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_CONVERSION_SUPPORT, &cs, sizeof(cs))))
	{
		SIMUL_CERR << "Error occurred checking decode conversion support of the video device.";
		return cp::VideoDecoderResult::VideoAPIError;
	}

	if (cs.SupportFlags != D3D12_VIDEO_DECODE_CONVERSION_SUPPORT_FLAG_SUPPORTED)
	{
		SIMUL_CERR << "The driver does not support conversion with the provided formats!";
		return cp::VideoDecoderResult::UnsupportedFeatures;
	}

	return cp::VideoDecoderResult::Ok;
}

void DecoderTexture::ChangeState(ID3D12VideoDecodeCommandList* commandList, bool write)
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
