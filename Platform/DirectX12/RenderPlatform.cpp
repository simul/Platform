#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "Simul/Platform/DirectX12/Texture.h"
#include "Simul/Platform/DirectX12/FramebufferDX1x.h"
#include "Simul/Platform/DirectX12/Effect.h"
#include "Simul/Platform/DirectX12/Buffer.h"
#include "Simul/Platform/DirectX12/Layout.h"
#include "Simul/Platform/DirectX12/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/DirectX12/CompileShaderDX1x.h"
#include "Simul/Platform/DirectX12/ConstantBuffer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/DirectX12/Utilities.h"
#include "Simul/Platform/DirectX12/Heap.h"
#include "Simul/Platform/DirectX12/Fence.h"


#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif

#include <algorithm>

using namespace simul;
using namespace dx12;


RenderPlatform::RenderPlatform():
	mCommandList(nullptr),
	m12Device(nullptr),
	mLastFrame(-1),
	mCurIdx(0),
	mTimeStampFreq(0),
	mSamplerHeap(nullptr),
	mRenderTargetHeap(nullptr),
	mDepthStencilHeap(nullptr),
	mFrameHeap(nullptr),
	mFrameSamplerHeap(nullptr)
{
	gpuProfiler = new crossplatform::GpuProfiler();
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::SetCommandList(ID3D12GraphicsCommandList * cmdList)
{
	mCommandList						= cmdList;
	immediateContext.platform_context	= cmdList;
	immediateContext.renderPlatform		= this;
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return mCommandList;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return m12Device;
}

void RenderPlatform::ResourceTransitionSimple(	ID3D12Resource * res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, 
												bool flush /*= false*/, UINT subRes /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		res,before,after,subRes
	);
	mPendingBarriers.push_back(barrier);

	if (flush)
	{
		FlushBarriers();
	}
}

void RenderPlatform::FlushBarriers()
{
	if (mPendingBarriers.empty())
	{
		return;
	}
	mCommandList->ResourceBarrier(mPendingBarriers.size(), &mPendingBarriers[0]);
	mPendingBarriers.clear();
}

void RenderPlatform::PushToReleaseManager(ID3D12DeviceChild* res, std::string dName)
{
	if (!res)
		return;
	// Don't add duplicates, this operation can be potentially slow if we have tons of resources
	for (unsigned int i = 0; i < mResourceBin.size(); i++)
	{
		if (res == mResourceBin[i].second.second)
			return;
	}
	mResourceBin.push_back(std::pair<int, std::pair<std::string, ID3D12DeviceChild*>>
	(
		0,
		std::pair<std::string, ID3D12DeviceChild*>
		(
			dName,
			res
		)
	));
}

void simul::dx12::RenderPlatform::ClearIA(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetVertexBuffers(0, 1, nullptr); // Only 1? 
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetIndexBuffer(nullptr);
}

void RenderPlatform::RestoreDeviceObjects(void* device)
{
	isInitialized	= true;
	mCurInputLayout	= nullptr;

	if (m12Device == device && device != nullptr)
	{
		return;
	}
	if (m12Device != device)
	{
		m12Device = (ID3D12Device*)device;
	}

	immediateContext.platform_context = mCommandList;

	// Create the frame heaps
	// These heaps will be shader visible as they will be the ones bound to the command list
	mFrameHeap			= new dx12::Heap[3];
	mFrameSamplerHeap	= new dx12::Heap[3];
	for (unsigned int i = 0; i < 3; i++)
	{
		mFrameHeap[i].Restore(this, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,"FrameHeap");
		mFrameSamplerHeap[i].Restore(this, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,"FrameSamplerHeap");
	}
	// Create storage heaps
	// These heaps wont be shader visible as they will be the source of the copy operation
	// we use these as storage
	mSamplerHeap = new dx12::Heap;
	mSamplerHeap->Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "SamplerHeap", false);
	mRenderTargetHeap = new dx12::Heap;
	mRenderTargetHeap->Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "RenderTargetsHeap", false);
	mDepthStencilHeap = new dx12::Heap;
	mDepthStencilHeap->Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "DepthStencilsHeap", false);

	// Create dummy textures
	mDummy2D = CreateTexture("Dummy2D");
	mDummy3D = CreateTexture("Dummy3D");
	mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);
	mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);


#ifdef _XBOX_ONE
	// Refer to UE4:(XboxOneD3D12Device.cpp) FXboxOneD3D12DynamicRHI::GetHardwareGPUFrameTime() 
	mTimeStampFreq					= D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY;
#else
	HRESULT res						= S_FALSE;
	D3D12_COMMAND_QUEUE_DESC qdesc	= {};
	qdesc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	qdesc.Type						= D3D12_COMMAND_LIST_TYPE_DIRECT;
	ID3D12CommandQueue* queue		= nullptr;
	res = m12Device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));
	SIMUL_ASSERT(res == S_OK);
	res = queue->GetTimestampFrequency(&mTimeStampFreq);
	SIMUL_ASSERT(res == S_OK);
	SAFE_RELEASE(queue);
#endif

	crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);
	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
}

void RenderPlatform::RecompileShaders()
{
	if(!m12Device)
		return;
	shaders.clear();
	crossplatform::RenderPlatform::RecompileShaders();
}

void RenderPlatform::BeginEvent			(crossplatform::DeviceContext &deviceContext,const char *name)
{
#ifdef SIMUL_WIN8_SDK

#endif
#ifdef SIMUL_ENABLE_PIX
	if(last_name==string(name))
		PIXBeginEvent( 0, name, name );
#endif
}

void RenderPlatform::EndEvent			(crossplatform::DeviceContext &deviceContext)
{
#ifdef SIMUL_WIN8_SDK

#endif
#ifdef SIMUL_ENABLE_PIX
	PIXEndEvent();
#endif
}

void RenderPlatform::StartRender(crossplatform::DeviceContext &deviceContext)
{
	// Store a reference to the device context
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_TEST_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_TEST_DEPTH_LESS_EQUAL);

	// Create dummy textures
	static bool creteDummy = true;
	if (creteDummy)
	{
		const uint_fast8_t dummyData[4] = { 0,0,0,0 };
		mDummy2D->setTexels(deviceContext, &dummyData[0], 0, 1);
		mDummy3D->setTexels(deviceContext, &dummyData[0], 0, 1);
		creteDummy = false;
	}

	// Age and delete old objects
	unsigned int kMaxAge = 4;
	if (!mResourceBin.empty())
	{
		for (int i = (int)(mResourceBin.size() - 1); i >= 0; i--)
		{
			mResourceBin[i].first++;
			if ((unsigned int)mResourceBin[i].first >= kMaxAge)
			{
				int remainRefs = 0;
				if(mResourceBin[i].second.second)
					remainRefs = mResourceBin[i].second.second->Release();
				/*
				if (remainRefs)
					SIMUL_CERR << mResourceBin[i].second.first << " is still referenced( " << remainRefs << " )" << std::endl;
				*/
				mResourceBin.erase(mResourceBin.begin() + i);
			}
		}
	}
}

void RenderPlatform::EndRender(crossplatform::DeviceContext &)
{

}

void RenderPlatform::IntializeLightingEnvironment(const float pAmbientLight[3])
{

}

void RenderPlatform::CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;

	auto src			= (dx12::Texture*)s;
	auto dst			= (dx12::Texture*)t;
	if (!src || !dst)
	{
		SIMUL_CERR << "Passed a null texture to CopyTexture(), ignoring call.\n";
		return;
	}

	// Ensure textures are compatible
	auto srcDesc = src->AsD3D12Resource()->GetDesc();
	auto dstDesc = dst->AsD3D12Resource()->GetDesc();
	if(	(srcDesc.Width != dstDesc.Width)						||
		(srcDesc.Height != dstDesc.Height)						||
		(srcDesc.DepthOrArraySize != dstDesc.DepthOrArraySize)	||
		(srcDesc.MipLevels != dstDesc.MipLevels))
	{
		SIMUL_CERR << "Passed incompatible textures to CopyTexture(), both textures should have same width,height,depth and mip level, ignoring call.\n";
		SIMUL_CERR << "Src: width: " << srcDesc.Width << ", height: " << srcDesc.Height << ", depth: " << srcDesc.DepthOrArraySize << ", mips: " << srcDesc.MipLevels << std::endl;
		SIMUL_CERR << "Dst: width: " << dstDesc.Width << ", height: " << dstDesc.Height << ", depth: " << dstDesc.DepthOrArraySize << ", mips: " << dstDesc.MipLevels << std::endl;
		return;
	}

	// Ensure source state
	bool changedSrc = false;
	auto srcState	= src->GetCurrentState();
	if ((srcState & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changedSrc = true;
		ResourceTransitionSimple(src->AsD3D12Resource(), srcState, D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	}
	
	// Ensure dst state
	bool changedDst = false;
	auto dstState	= dst->GetCurrentState();
	if ((dstState & D3D12_RESOURCE_STATE_COPY_DEST) != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		changedDst = true;
		ResourceTransitionSimple(dst->AsD3D12Resource(), dstState, D3D12_RESOURCE_STATE_COPY_DEST,true);
	}
	
	// Perform the copy. This is done GPU side and does not incur much CPU overhead (if copying full resources)
	mCommandList->CopyResource(dst->AsD3D12Resource(), src->AsD3D12Resource());
	
	// Reset to the original states
	if (changedSrc)
	{
		ResourceTransitionSimple(src->AsD3D12Resource(), D3D12_RESOURCE_STATE_COPY_SOURCE, srcState,true);
	}
	if (changedDst)
	{
		ResourceTransitionSimple(dst->AsD3D12Resource(), D3D12_RESOURCE_STATE_COPY_DEST, dstState,true);
	}
}


void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->Dispatch(w, l, d);
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	
}

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->DrawInstanced(num_verts,1,start_vert,0);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->DrawIndexedInstanced(num_indices, 1, start_index, base_vert, 0);
}

void RenderPlatform::DrawMarker(crossplatform::DeviceContext &deviceContext,const double *matrix)
{

}


void RenderPlatform::DrawCrossHair(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition)
{

}

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)
{

}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &deviceContext,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{

}

void RenderPlatform::ApplyDefaultMaterial()
{

}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	ERRNO_BREAK
	crossplatform::Texture * tex=NULL;
	tex=new dx12::Texture();
	if(fileNameUtf8&&strlen(fileNameUtf8)>0)
	{
		if(strstr( fileNameUtf8,".")!=nullptr)
			tex->LoadFromFile(this,fileNameUtf8);
		tex->SetName(fileNameUtf8);
	}
	
	ERRNO_BREAK
	return tex;
}

crossplatform::BaseFramebuffer *RenderPlatform::CreateFramebuffer(const char *name)
{
	return new dx12::Framebuffer(name);
}

static D3D12_TEXTURE_ADDRESS_MODE ToD3D12TextureAddressMode(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch (w)	
	{
	case simul::crossplatform::SamplerStateDesc::WRAP:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		break;
	case simul::crossplatform::SamplerStateDesc::CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		break;
	case simul::crossplatform::SamplerStateDesc::MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		break;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}
static D3D12_FILTER ToD3D12Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	switch (f)
	{
	case simul::crossplatform::SamplerStateDesc::POINT:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	case simul::crossplatform::SamplerStateDesc::LINEAR:
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	case simul::crossplatform::SamplerStateDesc::ANISOTROPIC:
		return D3D12_FILTER_ANISOTROPIC;
	default: 
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	}
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	const float border[4] = { 0.0f,0.0f,0.0f,0.0f };
	dx12::SamplerState *s = new dx12::SamplerState(d);

	D3D12_SAMPLER_DESC s12	= {};
	s12.Filter				= ToD3D12Filter(d->filtering);
	s12.AddressU			= ToD3D12TextureAddressMode(d->x);
	s12.AddressV			= ToD3D12TextureAddressMode(d->y);
	s12.AddressW			= ToD3D12TextureAddressMode(d->z);
	s12.ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	s12.MaxAnisotropy		= 16;
	s12.MinLOD				= 0.0f;
	s12.MaxLOD				= D3D12_FLOAT32_MAX;
	s12.MipLODBias			= 0.0f;
	
	m12Device->CreateSampler(&s12, mSamplerHeap->CpuHandle());
	s->SetDescriptorHandle( mSamplerHeap->CpuHandle());
	mSamplerHeap->Offset();

	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect()
{
	crossplatform::Effect *e= new dx12::Effect();
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx12::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx12::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx12::Buffer();
	return b;
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGB_11_11_10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RG_16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case RG_32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case INT_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_UNORM_COMPRESSED:
		return DXGI_FORMAT_BC7_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case R_32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RG_32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case D_16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case D_24_UNORM_S_8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	};
}

crossplatform::PixelFormat RenderPlatform::FromDxgiFormat(DXGI_FORMAT f)
{
	using namespace crossplatform;
	switch(f)
	{
	case DXGI_FORMAT_R16_FLOAT:
		return R_16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return RGBA_16_FLOAT;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return RGBA_32_FLOAT;
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return RGB_32_FLOAT;
	case DXGI_FORMAT_R32G32_FLOAT:
		return RG_32_FLOAT;
	case DXGI_FORMAT_R32_FLOAT:
		return R_32_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		// What possible reason is there for this to exist?
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R32_UINT:
		return R_32_UINT;
	case DXGI_FORMAT_R32G32_UINT:
		return RG_32_UINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return RGB_32_UINT;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return RGBA_32_UINT;
	case DXGI_FORMAT_D32_FLOAT:
		return D_32_FLOAT;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return D_24_UNORM_S_8_UINT;
	case DXGI_FORMAT_D16_UNORM:
		return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

crossplatform::ShaderResourceType RenderPlatform::FromD3DShaderVariableType(D3D_SHADER_VARIABLE_TYPE t)
{
	using namespace crossplatform;
	switch(t)
	{
	case D3D_SVT_TEXTURE1D:
		return ShaderResourceType::TEXTURE_1D;
	case D3D_SVT_TEXTURE2D:
		return ShaderResourceType::TEXTURE_2D;
	case D3D_SVT_TEXTURE3D:
		return ShaderResourceType::TEXTURE_3D;
	case D3D_SVT_TEXTURECUBE:
		return ShaderResourceType::TEXTURE_CUBE;
	case D3D_SVT_SAMPLER:
		return ShaderResourceType::SAMPLER;
	case D3D_SVT_BUFFER:
		return ShaderResourceType::BUFFER;
	case D3D_SVT_CBUFFER:
		return ShaderResourceType::CBUFFER;
	case D3D_SVT_TBUFFER:
		return ShaderResourceType::TBUFFER;
	case D3D_SVT_TEXTURE1DARRAY:
		return ShaderResourceType::TEXTURE_1D_ARRAY;
	case D3D_SVT_TEXTURE2DARRAY:
		return ShaderResourceType::TEXTURE_2D_ARRAY;
	case D3D_SVT_TEXTURE2DMS:
		return ShaderResourceType::TEXTURE_2DMS;
	case D3D_SVT_TEXTURE2DMSARRAY:
		return ShaderResourceType::TEXTURE_2DMS_ARRAY;
	case D3D_SVT_TEXTURECUBEARRAY:
		return ShaderResourceType::TEXTURE_CUBE_ARRAY;
	case D3D_SVT_RWTEXTURE1D:
		return ShaderResourceType::RW_TEXTURE_1D;
	case D3D_SVT_RWTEXTURE1DARRAY:
		return ShaderResourceType::RW_TEXTURE_1D_ARRAY;
	case D3D_SVT_RWTEXTURE2D:
		return ShaderResourceType::RW_TEXTURE_2D;
	case D3D_SVT_RWTEXTURE2DARRAY:
		return ShaderResourceType::RW_TEXTURE_2D_ARRAY;
	case D3D_SVT_RWTEXTURE3D:
		return ShaderResourceType::RW_TEXTURE_3D;
	case D3D_SVT_RWBUFFER:
		return ShaderResourceType::RW_BUFFER;
	case D3D_SVT_BYTEADDRESS_BUFFER:
		return ShaderResourceType::BYTE_ADDRESS_BUFFER;
	case D3D_SVT_RWBYTEADDRESS_BUFFER:
		return ShaderResourceType::RW_BYTE_ADDRESS_BUFFER;
	case D3D_SVT_STRUCTURED_BUFFER:
		return ShaderResourceType::STRUCTURED_BUFFER;
	case D3D_SVT_RWSTRUCTURED_BUFFER:
		return ShaderResourceType::RW_STRUCTURED_BUFFER;
	case D3D_SVT_APPEND_STRUCTURED_BUFFER:
		return ShaderResourceType::APPEND_STRUCTURED_BUFFER;
	case D3D_SVT_CONSUME_STRUCTURED_BUFFER:
		return ShaderResourceType::CONSUME_STRUCTURED_BUFFER;
	default:
		return ShaderResourceType::COUNT;
	}
}

int RenderPlatform::ByteSizeOfFormatElement(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 16;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 12;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return 8;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return 4;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return 2;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return 1;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		return 16;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_R1_UNORM:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 8;

		// Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return 4;

		// These are compressed, but bit-size information is unclear.
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
		return 4;

	case DXGI_FORMAT_UNKNOWN:
	default:
		return 0;
	}
}

bool simul::dx12::RenderPlatform::IsTypeless(DXGI_FORMAT fmt, bool partialTypeless)
{
	switch (static_cast<int>(fmt))
	{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC7_TYPELESS:
			return true;

		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return partialTypeless;

		case 119 /* DXGI_FORMAT_R16_UNORM_X8_TYPELESS */:
		case 120 /* DXGI_FORMAT_X16_TYPELESS_G8_UINT */:
			// These are Xbox One platform specific types
			return partialTypeless;

		default:
			return false;
	}
}

DXGI_FORMAT simul::dx12::RenderPlatform::TypelessToSrvFormat(DXGI_FORMAT fmt)
{
	if (!IsTypeless(fmt, true))
		return fmt;
	int u = fmt + 1;
	switch (fmt)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:break;
	};
	return (DXGI_FORMAT)u;
}

D3D12_QUERY_TYPE simul::dx12::RenderPlatform::ToD3dQueryType(crossplatform::QueryType t)
{
	switch (t)
	{
	case crossplatform::QUERY_OCCLUSION:
		return D3D12_QUERY_TYPE_OCCLUSION;
	case crossplatform::QUERY_TIMESTAMP:
		return D3D12_QUERY_TYPE_TIMESTAMP;
	default:
		return  D3D12_QUERY_TYPE_OCCLUSION;
	};
}

D3D12_QUERY_HEAP_TYPE simul::dx12::RenderPlatform::ToD3D12QueryHeapType(crossplatform::QueryType t)
{
	switch (t)
	{
	case simul::crossplatform::QUERY_UNKNWON:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case simul::crossplatform::QUERY_OCCLUSION:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	case simul::crossplatform::QUERY_TIMESTAMP:
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	case simul::crossplatform::QUERY_TIMESTAMP_DISJOINT:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;	
	default:
		return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	}
}

UINT RenderPlatform::GetResourceIndex(int mip, int layer, int mips, int layers)
{
	// Requested the whole resource
	if (mip == -1 && layer == -1)
	{
		return D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}
	int curMip		= (mip == -1) ? 0 : mip;
	int curLayer	= (layer == -1) ? 0 : layer;
	return D3D12CalcSubresource(curMip,curLayer, 0, mips, layers);
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc)
{
	dx12::Layout *l = new dx12::Layout();

	// Init and fill the input element desc array
	l->Dx12InputLayout.resize(num_elements);

	for (int i = 0; i < num_elements; i++)
	{
		if (strcmp(desc[i].semanticName, "POSITION") != 0)
		{
			//strcat(desc[i].semanticName, std::to_string(desc[i].semanticIndex).c_str());
		}
		l->Dx12InputLayout[i].SemanticName			= desc[i].semanticName;
		l->Dx12InputLayout[i].SemanticIndex			= desc[i].semanticIndex;
		l->Dx12InputLayout[i].Format				= ToDxgiFormat(desc[i].format);
		l->Dx12InputLayout[i].InputSlot				= desc[i].inputSlot;
		l->Dx12InputLayout[i].AlignedByteOffset		= desc[i].alignedByteOffset;
		l->Dx12InputLayout[i].InputSlotClass		= desc[i].perInstance ?	D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA :D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		l->Dx12InputLayout[i].InstanceDataStepRate	= desc[i].instanceDataStepRate;
	}

	// Fill the input layout description
	l->Dx12LayoutDesc.pInputElementDescs	= l->Dx12InputLayout.data();
	l->Dx12LayoutDesc.NumElements			= num_elements;

	l->SetDesc(desc, num_elements);

	return l;
}

static D3D12_COMPARISON_FUNC toD3d12Comparison(crossplatform::DepthComparison d)
{
	switch (d)
	{
	case simul::crossplatform::DEPTH_ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	case simul::crossplatform::DEPTH_NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case simul::crossplatform::DEPTH_LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case simul::crossplatform::DEPTH_EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case simul::crossplatform::DEPTH_LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case simul::crossplatform::DEPTH_GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case simul::crossplatform::DEPTH_NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case simul::crossplatform::DEPTH_GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	default:
		SIMUL_BREAK("Undefined comparison func");
		return D3D12_COMPARISON_FUNC_NEVER;
	}
}

static D3D12_BLEND_OP toD3d12BlendOp(crossplatform::BlendOperation o)
{
	switch (o)
	{
	case simul::crossplatform::BLEND_OP_NONE:
		return D3D12_BLEND_OP_ADD; // Just pass a default thing?
	case simul::crossplatform::BLEND_OP_ADD:
		return D3D12_BLEND_OP_ADD;
	case simul::crossplatform::BLEND_OP_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case simul::crossplatform::BLEND_OP_MAX:
		return D3D12_BLEND_OP_MAX;
	case simul::crossplatform::BLEND_OP_MIN:
		return D3D12_BLEND_OP_MIN;
	default:
		SIMUL_BREAK("Undefined blend operation");
		return D3D12_BLEND_OP_ADD;;
	}
}

static D3D12_BLEND toD3d12Blend(crossplatform::BlendOption o)
{
	switch (o)
	{
	case simul::crossplatform::BLEND_ZERO:
		return D3D12_BLEND_ZERO;
	case simul::crossplatform::BLEND_ONE:
		return D3D12_BLEND_ONE;
	case simul::crossplatform::BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case simul::crossplatform::BLEND_INV_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case simul::crossplatform::BLEND_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case simul::crossplatform::BLEND_INV_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case simul::crossplatform::BLEND_DEST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case simul::crossplatform::BLEND_INV_DEST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	case simul::crossplatform::BLEND_DEST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case simul::crossplatform::BLEND_INV_DEST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case simul::crossplatform::BLEND_SRC_ALPHA_SAT:
		return D3D12_BLEND_SRC_ALPHA_SAT;
	case simul::crossplatform::BLEND_BLEND_FACTOR:
		return D3D12_BLEND_BLEND_FACTOR;
	case simul::crossplatform::BLEND_INV_BLEND_FACTOR:
		return D3D12_BLEND_INV_BLEND_FACTOR;
	case simul::crossplatform::BLEND_SRC1_COLOR:
		return D3D12_BLEND_SRC1_COLOR;
	case simul::crossplatform::BLEND_INV_SRC1_COLOR:
		return D3D12_BLEND_INV_SRC1_COLOR;
	case simul::crossplatform::BLEND_SRC1_ALPHA:
		return D3D12_BLEND_SRC1_ALPHA;
	case simul::crossplatform::BLEND_INV_SRC1_ALPHA:
		return D3D12_BLEND_INV_SRC1_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

D3D12_FILL_MODE toD3d12FillMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return D3D12_FILL_MODE_SOLID;
	case crossplatform::POLYGON_MODE_LINE:
		return D3D12_FILL_MODE_WIREFRAME;
	case crossplatform::POLYGON_MODE_POINT:
		SIMUL_BREAK("Directx12 doesn'thave a POINT polygon mode");
		return D3D12_FILL_MODE_SOLID;
	default:
		SIMUL_BREAK("Undefined polygon mode");
		return D3D12_FILL_MODE_SOLID;
	}
}

D3D12_CULL_MODE toD3d12CullMode(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case simul::crossplatform::CULL_FACE_NONE:
		return D3D12_CULL_MODE_NONE;
	case simul::crossplatform::CULL_FACE_FRONT:
		return D3D12_CULL_MODE_FRONT;
	case simul::crossplatform::CULL_FACE_BACK:
		return D3D12_CULL_MODE_BACK;
	case simul::crossplatform::CULL_FACE_FRONTANDBACK:
		SIMUL_BREAK("In Directx12 there is no FRONTANDBACK cull face mode");
	default:
		SIMUL_BREAK("Undefined cull face mode");
		return D3D12_CULL_MODE_NONE;
	}
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx12::RenderState* s= new dx12::RenderState();
	s->type				= desc.type;

	if (desc.type == crossplatform::BLEND)
	{
		s->BlendDesc						= D3D12_BLEND_DESC();
		s->BlendDesc.AlphaToCoverageEnable	= desc.blend.AlphaToCoverageEnable;
		s->BlendDesc.IndependentBlendEnable = desc.blend.IndependentBlendEnable;
		for (int i = 0; i < desc.blend.numRTs; i++)	// TO-DO: Check for max simultaneos rt value?
		{
			s->BlendDesc.RenderTarget[i].BlendEnable			=	(desc.blend.RenderTarget[i].blendOperation		!= crossplatform::BLEND_OP_NONE) || 
																	(desc.blend.RenderTarget[i].blendOperationAlpha != crossplatform::BLEND_OP_NONE);
			s->BlendDesc.RenderTarget[i].BlendOp				= toD3d12BlendOp(desc.blend.RenderTarget[i].blendOperation);
			s->BlendDesc.RenderTarget[i].SrcBlend				= toD3d12Blend(desc.blend.RenderTarget[i].SrcBlend);
			s->BlendDesc.RenderTarget[i].DestBlend				= toD3d12Blend(desc.blend.RenderTarget[i].DestBlend);

			s->BlendDesc.RenderTarget[i].BlendOpAlpha			= toD3d12BlendOp(desc.blend.RenderTarget[i].blendOperationAlpha);
			s->BlendDesc.RenderTarget[i].SrcBlendAlpha			= toD3d12Blend(desc.blend.RenderTarget[i].SrcBlendAlpha);
			s->BlendDesc.RenderTarget[i].DestBlendAlpha			= toD3d12Blend(desc.blend.RenderTarget[i].DestBlendAlpha);

			s->BlendDesc.RenderTarget[i].RenderTargetWriteMask	= desc.blend.RenderTarget[i].RenderTargetWriteMask;
			s->BlendDesc.RenderTarget[i].LogicOpEnable			= false;
			s->BlendDesc.RenderTarget[i].LogicOp				= D3D12_LOGIC_OP_NOOP;
		}
	}
	else if (desc.type == crossplatform::RASTERIZER)
	{
		s->RasterDesc							= D3D12_RASTERIZER_DESC();
		s->RasterDesc.FillMode					= toD3d12FillMode(desc.rasterizer.polygonMode);
		s->RasterDesc.CullMode					= toD3d12CullMode(desc.rasterizer.cullFaceMode);
		s->RasterDesc.FrontCounterClockwise		= desc.rasterizer.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE;
		s->RasterDesc.DepthBias					= 0;
		s->RasterDesc.DepthBiasClamp			= 0.0f;
		s->RasterDesc.SlopeScaledDepthBias		= 0.0f;
		s->RasterDesc.DepthClipEnable			= false;
		s->RasterDesc.MultisampleEnable			= false;	// TO-DO : what if not!!!
		s->RasterDesc.AntialiasedLineEnable		= false;
		s->RasterDesc.ForcedSampleCount			= 0;
		s->RasterDesc.ConservativeRaster		= D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}
	else if (desc.type == crossplatform::DEPTH)
	{
		s->DepthStencilDesc						= D3D12_DEPTH_STENCIL_DESC();
		s->DepthStencilDesc.DepthEnable			= desc.depth.test;
		s->DepthStencilDesc.DepthWriteMask		= desc.depth.write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		s->DepthStencilDesc.DepthFunc			= toD3d12Comparison(desc.depth.comparison);
		s->DepthStencilDesc.StencilEnable		= false;
	}
	else
	{
		SIMUL_BREAK("Not recognised render state type");
	}
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	dx12::Query* q = new dx12::Query(type);
	return q;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	,crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	if (buffers == nullptr)
		return;

	if (num_buffers > 1)
	{
		SIMUL_BREAK("Nacho has to work to do here!!");
	}
	auto pBuffer = (dx12::Buffer*)buffers[0];
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetVertexBuffers
	(
		0,
		num_buffers,
		pBuffer->GetVertexBufferView()
	);
};

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *vertexBuffer,int start_index)
{

}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext &deviceContext)
{
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;

	SIMUL_ASSERT(num <= D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

	D3D12_VIEWPORT viewports[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]	= {};
	D3D12_RECT	   scissors[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]	= {};

	for(int i=0;i<num;i++)
	{
		// Configure viewports
		viewports[i].Width		= (float)vps[i].w;
		viewports[i].Height		= (float)vps[i].h;
		viewports[i].TopLeftX	= (float)vps[i].x;
		viewports[i].TopLeftY	= (float)vps[i].y;
		viewports[i].MinDepth	= vps[i].znear;
		viewports[i].MaxDepth	= vps[i].zfar;

		// Configure scissor
		scissors[i].left		= 0;
		scissors[i].top			= 0;
		scissors[i].right		= (LONG)viewports[i].Width;
		scissors[i].bottom		= (LONG)viewports[i].Height;
	}

	mCommandList->RSSetViewports(num, viewports);
	mCommandList->RSSetScissorRects(num, scissors);
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	deviceContext.renderPlatform->AsD3D12CommandList()->IASetIndexBuffer(buffer->GetIndexBufferView());
}

static D3D_PRIMITIVE_TOPOLOGY toD3dTopology(crossplatform::Topology t)
{	
	using namespace crossplatform;
	switch(t)
	{			
	case POINTLIST:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case LINELIST:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case LINESTRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TRIANGLELIST:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TRIANGLESTRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case LINELIST_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case LINESTRIP_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case TRIANGLELIST_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case TRIANGLESTRIP_ADJ:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	default:
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;

	D3D_PRIMITIVE_TOPOLOGY T = toD3dTopology(t);
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetPrimitiveTopology(T);
	mStoredTopology = T;
}

void RenderPlatform::SetLayout(crossplatform::DeviceContext &deviceContext,crossplatform::Layout *l)
{
	mCommandList = deviceContext.asD3D12Context();
	immediateContext = deviceContext;

	if (l)
	{
		l->Apply(deviceContext);
	}
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;
	
	if (s->type == crossplatform::BLEND)
	{
		const float blendFactor[] = { 0.0f,0.0f,0.0f,0.0f };
		mCommandList->OMSetBlendFactor(&blendFactor[0]);
	}
	if (s->type == crossplatform::DEPTH)
	{
		mCommandList->OMSetStencilRef(0);
	}
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{

}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext& deviceContext, bool error_checking)
{
	mCommandList					= deviceContext.asD3D12Context();
	immediateContext				= deviceContext;
	crossplatform::ContextState *cs = GetContextState(deviceContext);
	if (!cs || !cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}

	dx12::EffectPass *pass = static_cast<dx12::EffectPass*>(cs->currentEffectPass);
	if (!cs->effectPassValid)
	{
		if (cs->last_action_was_compute&&pass->shaders[crossplatform::SHADERTYPE_VERTEX] != nullptr)
		{
			cs->last_action_was_compute = false;
		}
		else if ((!cs->last_action_was_compute) && pass->shaders[crossplatform::SHADERTYPE_COMPUTE] != nullptr)
		{
			cs->last_action_was_compute = true;
		}
		// This applies the pass, and also any associated state: Blend, Depth and Rasterizer states:
		pass->Apply(deviceContext, cs->last_action_was_compute);
		cs->effectPassValid = true;
	}

	// We will only set the tables once per frame
	if (mLastFrame != deviceContext.frame_number)
	{
		// Call start render at least once per frame to make sure the bins 
		// release objects!
		StartRender(deviceContext);

		mLastFrame = deviceContext.frame_number;
		mCurIdx++;
		mCurIdx = mCurIdx % kNumIdx;

		// Reset the frame heaps (SRV_CBV_UAV and SAMPLER)
		mFrameHeap[mCurIdx].Reset();
		mFrameSamplerHeap[mCurIdx].Reset();
	}

	// Set the frame descriptor heaps
	// TO-DO: this should be done ONLY once per frame
	// but that is causing some trouble with UE4
	ID3D12DescriptorHeap* currentHeaps[2] =
	{
		mFrameHeap[mCurIdx].GetHeap(),
		mFrameSamplerHeap[mCurIdx].GetHeap()
	};
	deviceContext.asD3D12Context()->SetDescriptorHeaps(2, currentHeaps);

	// CBV_SRV_UAV table
	if (pass->usesConstantBuffers() || pass->usesTextures() || pass->usesSBs())
	{
		if (pass->IsCompute())
		{
			deviceContext.asD3D12Context()->SetComputeRootDescriptorTable(pass->ResourceTableIndex(), mFrameHeap[mCurIdx].GpuHandle());
		}
		else
		{
			deviceContext.asD3D12Context()->SetGraphicsRootDescriptorTable(pass->ResourceTableIndex(), mFrameHeap[mCurIdx].GpuHandle());
		}

	}
	// SAMPLER table
	if (pass->usesSamplers())
	{
		if (pass->IsCompute())
		{
			deviceContext.asD3D12Context()->SetComputeRootDescriptorTable(pass->SamplerTableIndex(), mFrameSamplerHeap[mCurIdx].GpuHandle());
		}
		else
		{
			deviceContext.asD3D12Context()->SetGraphicsRootDescriptorTable(pass->SamplerTableIndex(), mFrameSamplerHeap[mCurIdx].GpuHandle());
		}
	}

	/*
		NOTE: the order in which we set the different resources should match the order
		we used to create the RootSignature:

		Samplers
		Constant Buffers
		Textures
			SRV
			UAV
		Structured Buffers
			SRV
			UAV
	
		The resources will be ordered (based on slot index) inside the Set() Call.
	*/

	// Apply Samplers:	
	if (pass->usesSamplers())
	{
		auto dx12Effect = (dx12::Effect*)cs->currentEffect;
		pass->SetSamplers(dx12Effect->GetSamplers(),&mFrameSamplerHeap[mCurIdx],m12Device);
	}
	
	// Apply Constant Buffers:
	if (!cs->constantBuffersValid && pass->usesConstantBuffers())
	{
		pass->SetConstantBuffers(cs->applyBuffers, &mFrameHeap[mCurIdx],m12Device,deviceContext);
	}

	// Apply textures:
	if (!cs->textureAssignmentMapValid && pass->usesTextures())
	{
		pass->SetTextures(cs->textureAssignmentMap, &mFrameHeap[mCurIdx], m12Device, deviceContext);
	}
	if (!cs->rwTextureAssignmentMapValid && pass->usesRwTextures())
	{
		pass->SetRWTextures(cs->rwTextureAssignmentMap, &mFrameHeap[mCurIdx], m12Device, deviceContext);
	}

	// Apply StructuredBuffers:
	if (pass->usesSBs())
	{
		pass->SetStructuredBuffers(cs->applyStructuredBuffers, &mFrameHeap[mCurIdx], m12Device, deviceContext);
	}

	// TO-DO: Apply streamout targets
	if (!cs->streamoutTargetsValid)
	{
		for (auto i = cs->streamoutTargets.begin(); i != cs->streamoutTargets.end(); i++)
		{
			int slot = i->first;
			dx12::Buffer *vertexBuffer = (dx12::Buffer*)i->second;
			if (!vertexBuffer)
				continue;

			ID3D11Buffer *b = vertexBuffer->AsD3D11Buffer();
			if (!b)
				continue;
		}
		cs->streamoutTargetsValid = true;
	}

	// TO-DO: Apply vertex buffers
	if (!cs->vertexBuffersValid)
	{
		for (auto i : cs->applyVertexBuffers)
		{
			//if(pass->UsesConstantBufferSlot(i.first))
			dx12::Buffer* b = (dx12::Buffer*)(i.second);

			auto B=b->AsD3D11Buffer();
			//d3d11DeviceContext->IASetVertexBuffers( i.first, b->count,&B,nullptr,nullptr);
		}
		cs->vertexBuffersValid = true;
	}

	// We are ready to draw/dispatch, so now flush the barriers!
	FlushBarriers();

	return true;
}

#pragma optimize("", off)
void RenderPlatform::ClearTexture(crossplatform::DeviceContext &deviceContext, crossplatform::Texture *texture, const vec4& colour)
{
	// Silently return if not initialized
	if (!texture->IsValid())
		return;
	
	bool cleared				= false;
	debugConstants.debugColour	= colour;
	debugConstants.texSize		= uint4(texture->width, texture->length, texture->depth, 1);
	debugEffect->SetConstantBuffer(deviceContext, &debugConstants);

	// Compute clear
	if (texture->IsComputable())
	{
		int a = texture->NumFaces();
		if (a == 0)
			a = 1;
		for (int i = 0; i<a; i++)
		{
			int w = texture->width;
			int l = texture->length;
			int d = texture->depth;
			for (int j = 0; j<texture->mips; j++)
			{
				const char *techname = "compute_clear";
				int W = (w + 4 - 1) / 4;
				int L = (l + 4 - 1) / 4;
				int D = d;
				if (texture->dim == 2 && texture->NumFaces()>1)
				{
					W = (w + 8 - 1) / 8;
					L = (l + 8 - 1) / 8;
					D = d;
					techname = "compute_clear_2d_array";
					if (texture->GetFormat() == crossplatform::PixelFormat::RGBA_8_UNORM)
					{
						techname = "compute_clear_2d_array_u8";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget2DArrayU8", texture, i);
					}
					else
					{
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget2DArray", texture, i);
					}
				}
				else if (texture->dim == 2)
				{
					W = (w + 8 - 1) / 8;
					L = (l + 8 - 1) / 8;
					D = 1;
					debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget", texture, i, j);
				}
				else if (texture->dim == 3)
				{
					if (texture->GetFormat() == crossplatform::PixelFormat::RGBA_8_UNORM)
					{
						techname = "compute_clear_3d_u8";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3DU8", texture, i);
					}
					else
					{
						techname = "compute_clear_3d";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3D", texture, i);
					}
				}
				else
				{
					SIMUL_CERR_ONCE << ("Can't clear texture dim.\n	");
				}
				debugEffect->Apply(deviceContext, techname, 0);
				DispatchCompute(deviceContext, W, L, D);

				debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget", nullptr);
				debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3D", nullptr);
				w /= 2;
				l /= 2;
				d /= 2;
				debugEffect->Unapply(deviceContext);
			}
		}
		debugEffect->UnbindTextures(deviceContext);
		cleared = true;
	}
	// Render target clear
	else if (texture->HasRenderTargets() && !cleared)
	{
		int total_num = texture->arraySize*(texture->IsCubemap() ? 6 : 1);
		for (int i = 0; i<total_num; i++)
		{
			for (int j = 0; j<texture->mips; j++)
			{
				texture->activateRenderTarget(deviceContext, i, j);
				debugEffect->Apply(deviceContext, "clear", 0);
				DrawQuad(deviceContext);
				debugEffect->Unapply(deviceContext);
				texture->deactivateRenderTarget(deviceContext);
			}
		}
		debugEffect->UnbindTextures(deviceContext);
		cleared = true;
	}
	// Couldn't clear the texture
	else
	{
		SIMUL_CERR_ONCE << ("No method was found to clear this texture.\n");
	}
}
#pragma optimize("", on)
void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;
}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{

}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend/*=false*/,float gamma, bool debug)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;

	crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend, gamma, debug);
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	mCommandList		= deviceContext.asD3D12Context();
	immediateContext	= deviceContext;

	SetTopology(deviceContext,simul::crossplatform::TRIANGLESTRIP);
	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->DrawInstanced(4, 1, 0, 0);
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *line_vertices,int vertex_count,bool strip,bool test_depth,bool view_centred)
{

}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip)
{
	
}

void RenderPlatform::DrawCube(crossplatform::DeviceContext &deviceContext)
{
	
}

void RenderPlatform::PushRenderTargets(crossplatform::DeviceContext &deviceContext)
{
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &deviceContext)
{
}

crossplatform::Shader *RenderPlatform::CreateShader()
{
	Shader *S = new Shader();
	return S;
}
