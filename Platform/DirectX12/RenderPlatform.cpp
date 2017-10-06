#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "Simul/Platform/DirectX12/Material.h"
#include "Simul/Platform/DirectX12/Mesh.h"
#include "Simul/Platform/DirectX12/Texture.h"
#include "Simul/Platform/DirectX12/ESRAMTexture.h"
#include "Simul/Platform/DirectX12/FramebufferDX1x.h"
#include "Simul/Platform/DirectX12/Light.h"
#include "Simul/Platform/DirectX12/Effect.h"
#include "Simul/Platform/DirectX12/Buffer.h"
#include "Simul/Platform/DirectX12/Layout.h"
#include "Simul/Platform/DirectX12/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX12/CompileShaderDX1x.h"
#include "Simul/Platform/DirectX12/ConstantBuffer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "D3dx11effect.h"
#include <D3Dcompiler.h>
#include <algorithm>
#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif

#include "Simul/Platform/DirectX12/Utilities.h"

#include "d3d12.h"

using namespace simul;
using namespace dx12;

#pragma optimize("",off)

RenderPlatform::RenderPlatform():
	pUserDefinedAnnotation(nullptr),
	mCommandList(nullptr),
	m12Device(nullptr),
	mLastFrame(-1),
	mCurIdx(0)
{
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return mCommandList;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return m12Device;
}

void RenderPlatform::ResourceTransitionSimple(ID3D12Resource * res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subRes /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		res,before,after,subRes
	);
	mCommandList->ResourceBarrier(1, &barrier);
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

#ifdef _XBOX_ONE
ESRAMManager *eSRAMManager=NULL;
#endif
void RenderPlatform::RestoreDeviceObjects(void* device)
{
	isInitialized = true;
	mCurInputLayout = nullptr;

	// TO-DO: Check this
	if (m12Device == device && device != nullptr)
	{
		return;
	}
	if (m12Device != device)
	{
		m12Device = (ID3D12Device*)device;
	}

	// Create the frame heaps
	// These heaps will be shader visible as they will be the ones bound to the command list
	for (unsigned int i = 0; i < 3; i++)
	{
		mFrameHeap[i].Restore(this, 2048, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,"FrameHeap");
		mFrameSamplerHeap[i].Restore(this, 1024, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,"FrameSamplerHeap");
	}
	// Create storage heaps
	// These heaps wont be shader visible as they will be the source of the copy operation
	// we use these as storage
	mSamplerHeap.Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "SamplerHeap", false);
	mRenderTargetHeap.Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "RenderTargetsHeap", false);
	mDepthStencilHeap.Restore(this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "DepthStencilsHeap", false);

	// Create dummy textures
	mDummy2D = CreateTexture("Dummy2D");
	mDummy3D = CreateTexture("Dummy3D");
	mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);
	mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);

	crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);

	RecompileShaders();
	immediateContext.platform_context=mCommandList;
}

void RenderPlatform::InvalidateDeviceObjects()
{
#ifdef _XBOX_ONE
	delete eSRAMManager;
#endif
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx12::Material *mat=(dx12::Material*)(*i);
		delete mat;
	}
	materials.clear();
	SAFE_DELETE(debugEffect);
	ID3D11DeviceContext* c=immediateContext.asD3D11DeviceContext();
	SAFE_RELEASE(c);
	immediateContext.platform_context=NULL;
}

void RenderPlatform::RecompileShaders()
{
	if(!m12Device)
		return;
	crossplatform::RenderPlatform::RecompileShaders();
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx12::Material *mat=(dx12::Material*)(*i);
		mat->SetEffect(solidEffect);
	}
}

void RenderPlatform::BeginEvent			(crossplatform::DeviceContext &deviceContext,const char *name)
{
#ifdef SIMUL_WIN8_SDK
		if(pUserDefinedAnnotation)
			pUserDefinedAnnotation->BeginEvent(base::StringToWString(name).c_str());
#endif
#ifdef SIMUL_ENABLE_PIX
	if(last_name==string(name))
		PIXBeginEvent( 0, name, name );
#endif
}

void RenderPlatform::EndEvent			(crossplatform::DeviceContext &deviceContext)
{
#ifdef SIMUL_WIN8_SDK
	if(pUserDefinedAnnotation)
		pUserDefinedAnnotation->EndEvent();
#endif
#ifdef SIMUL_ENABLE_PIX
	PIXEndEvent();
#endif
}

void RenderPlatform::StartRender(crossplatform::DeviceContext &deviceContext)
{
	// Store a reference to the device context
	mCommandList = deviceContext.asD3D12Context();

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
	unsigned int kMaxAge = 6;
	if (!mResourceBin.empty())
	{
		for (int i = mResourceBin.size() - 1; i >= 0; i--)
		{
			mResourceBin[i].first++;
			if (mResourceBin[i].first >= kMaxAge)
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
	/*
		Read comment of dx12::getTexel about copying resources in DX12
	*/
	SIMUL_BREAK_ONCE("Not implemented.");
}

D3D11_MAP_FLAG RenderPlatform::GetMapFlags()
{
#ifdef _XBOX_ONE
	if(UsesFastSemantics())
		return D3D11_MAP_FLAG_ALLOW_USAGE_DEFAULT;
	else
#endif
		return (D3D11_MAP_FLAG)0;
}

void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	mCommandList = deviceContext.asD3D12Context();

	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->Dispatch(w, l, d);
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	
}

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	mCommandList = deviceContext.asD3D12Context();

	ClearIA(deviceContext);
	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->DrawInstanced(num_verts,1,start_vert,0);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	mCommandList = deviceContext.asD3D12Context();

	ClearIA(deviceContext);
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

crossplatform::Material *RenderPlatform::CreateMaterial()
{
	dx12::Material *mat=new dx12::Material;
	mat->SetEffect(solidEffect);
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new dx12::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new dx12::Light();
}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	ERRNO_BREAK
	crossplatform::Texture * tex=NULL;
#ifdef _XBOX_ONE
	//if(fileNameUtf8&&strcmp(fileNameUtf8,"ESRAM")==0)
	if(eSRAMManager&&eSRAMManager->IsEnabled())
		tex=new dx11::ESRAMTexture();
	else
#endif
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
		return D3D12_FILTER_ANISOTROPIC;
	case simul::crossplatform::SamplerStateDesc::ANISOTROPIC:
		return D3D12_FILTER_ANISOTROPIC;
	default: 
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	}
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	dx12::SamplerState *s = new dx12::SamplerState(d);

	D3D12_SAMPLER_DESC s12	= {};
	s12.Filter				= ToD3D12Filter(d->filtering);
	s12.AddressU			= ToD3D12TextureAddressMode(d->x);
	s12.AddressV			= ToD3D12TextureAddressMode(d->y);
	s12.AddressW			= ToD3D12TextureAddressMode(d->z);
	s12.ComparisonFunc		= D3D12_COMPARISON_FUNC_NEVER;
	s12.MaxAnisotropy		= 16;
	s12.MinLOD				= 0;
	s12.MaxLOD				= D3D12_FLOAT32_MAX;

	// Store the CPU handle
	m12Device->CreateSampler(&s12, mSamplerHeap.CpuHandle());
	s->mCpuHandle = mSamplerHeap.CpuHandle();
	mSamplerHeap.Offset();

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

UINT RenderPlatform::GetResourceIndex(int mip, int layer, int mips, int layers)
{
	// Requested the hole resource
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

#ifdef DO_DX12
	
	// Init and fill the input element desc array
	l->Dx12InputLayout.resize(num_elements);

	for (unsigned int i = 0; i < num_elements; i++)
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

#else

	D3D11_INPUT_ELEMENT_DESC *decl=new D3D11_INPUT_ELEMENT_DESC[num_elements];
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
		D3D11_INPUT_ELEMENT_DESC &D=decl[i];
		D.SemanticName=d.semanticName;
		D.SemanticIndex=d.semanticIndex;
		D.Format=ToDxgiFormat(d.format);
		D.AlignedByteOffset=d.alignedByteOffset;
		D.InputSlot=d.inputSlot;
		D.InputSlotClass=d.perInstance?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
		D.InstanceDataStepRate=d.instanceDataStepRate;
	};
	l->SetDesc(desc,num_elements);
	ID3DBlob *VS;
	ID3DBlob *errorMsgs=NULL;
	std::string dummy_shader;
	dummy_shader="struct vertexInput\n"
				"{\n";
	for(int i=0;i<num_elements;i++)
	{
		D3D11_INPUT_ELEMENT_DESC &dec=decl[i];
		std::string format;
		switch(dec.Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			format="float4";
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			format="float3";
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
			format="float2";
			break;
		case DXGI_FORMAT_R32_FLOAT:
			format="float";
			break;
		case DXGI_FORMAT_R32_UINT:
			format="uint";
			break;
		default:
			SIMUL_CERR<<"Unhandled type "<<std::endl;
		};
		dummy_shader+="   ";
		dummy_shader+=format+" ";
		std::string name=dec.SemanticName;
		if(strcmp(dec.SemanticName,"POSITION")!=0)
			name+=('0'+dec.SemanticIndex);
		dummy_shader+=name;
		dummy_shader+="_";
		dummy_shader+=" : ";
		dummy_shader+=name;
		dummy_shader+=";\n";
				//"	float3 position		: POSITION;"
				//"	float texCoords		: TEXCOORD0;";
	}
	dummy_shader+="};\n"
				"struct vertexOutput\n"
				"{\n"
				"	float4 hPosition	: SV_POSITION;\n"
				"};\n"
				"vertexOutput VS_Main(vertexInput IN)\n"
				"{\n"
				"	vertexOutput OUT;\n"
				"	OUT.hPosition	=float4(1.0,1.0,1.0,1.0);\n"
				"	return OUT;\n"
				"}\n";
	const char *str=dummy_shader.c_str();
	size_t len=strlen(str);
#if WINVER<0x602
	HRESULT hr=D3DX11CompileFromMemory(str,len,"dummy",NULL,NULL,"VS_Main", "vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, 0, &VS, &errorMsgs, 0);
#else
	HRESULT hr=D3DCompile(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, &VS, &errorMsgs);
#endif
	if(hr!=S_OK)
	{
		const char *e=(const char*)errorMsgs->GetBufferPointer();
		std::cerr<<e<<std::endl;
	}
	SAFE_RELEASE(l->d3d11InputLayout);
	AsD3D11Device()->CreateInputLayout(decl, num_elements, VS->GetBufferPointer(), VS->GetBufferSize(), &l->d3d11InputLayout);
	
	if(VS)
		VS->Release();
	if(errorMsgs)
		errorMsgs->Release();
	delete [] decl;
	return l;
#endif // DO_DX12

}
static D3D11_COMPARISON_FUNC toD3dComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_LESS:
		return D3D11_COMPARISON_LESS;
	case crossplatform::DEPTH_EQUAL:
		return D3D11_COMPARISON_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return D3D11_COMPARISON_LESS_EQUAL;
	case crossplatform::DEPTH_GREATER:
		return D3D11_COMPARISON_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return D3D11_COMPARISON_NOT_EQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return D3D11_COMPARISON_GREATER_EQUAL;
	default:
		break;
	};
	return D3D11_COMPARISON_LESS;
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

static D3D11_BLEND_OP toD3dBlendOp(crossplatform::BlendOperation o)
{
	switch(o)
	{
	case crossplatform::BLEND_OP_ADD:
		return D3D11_BLEND_OP_ADD;
	case crossplatform::BLEND_OP_SUBTRACT:
		return D3D11_BLEND_OP_SUBTRACT;
	case crossplatform::BLEND_OP_MAX:
		return D3D11_BLEND_OP_MAX;
	case crossplatform::BLEND_OP_MIN:
		return D3D11_BLEND_OP_MIN;
	default:
		break;
	};
	return D3D11_BLEND_OP_ADD;
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

static D3D11_BLEND toD3dBlend(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return D3D11_BLEND_ZERO;
	case crossplatform::BLEND_ONE:
		return D3D11_BLEND_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return D3D11_BLEND_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return D3D11_BLEND_INV_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return D3D11_BLEND_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return D3D11_BLEND_INV_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return D3D11_BLEND_DEST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return D3D11_BLEND_INV_DEST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return D3D11_BLEND_DEST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return D3D11_BLEND_INV_DEST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return D3D11_BLEND_SRC_ALPHA_SAT;
	case crossplatform::BLEND_BLEND_FACTOR:
		return D3D11_BLEND_BLEND_FACTOR;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return D3D11_BLEND_INV_BLEND_FACTOR;
	case crossplatform::BLEND_SRC1_COLOR:
		return D3D11_BLEND_SRC1_COLOR;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return D3D11_BLEND_INV_SRC1_COLOR;
	case crossplatform::BLEND_SRC1_ALPHA:
		return D3D11_BLEND_SRC1_ALPHA;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return D3D11_BLEND_INV_SRC1_ALPHA;
	default:
		break;
	};
	return D3D11_BLEND_ONE;
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

D3D11_FILL_MODE toD3dFillMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return D3D11_FILL_MODE::D3D11_FILL_SOLID;
	case crossplatform::POLYGON_MODE_LINE:
	default:
		return D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;
		break;
	};
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

D3D11_CULL_MODE toD3dCullMode(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case crossplatform::CullFaceMode::CULL_FACE_BACK:
		return D3D11_CULL_MODE::D3D11_CULL_BACK;
	case crossplatform::CullFaceMode::CULL_FACE_FRONT:
		return D3D11_CULL_MODE::D3D11_CULL_FRONT;
	case crossplatform::CullFaceMode::CULL_FACE_FRONTANDBACK:
		return D3D11_CULL_MODE::D3D11_CULL_BACK;
	case crossplatform::CullFaceMode::CULL_FACE_NONE:
		return D3D11_CULL_MODE::D3D11_CULL_NONE;
	default:
		return D3D11_CULL_MODE::D3D11_CULL_NONE;
	};
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
	dx12::RenderState *s=new dx12::RenderState();
	s->type=desc.type;


	if (desc.type == crossplatform::BLEND)
	{
		s->BlendDesc.AlphaToCoverageEnable = desc.blend.AlphaToCoverageEnable;
		s->BlendDesc.IndependentBlendEnable = desc.blend.IndependentBlendEnable;
		for (unsigned int i = 0; i < desc.blend.numRTs; i++)	// TO-DO: Check for max simultaneos rt value?
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
			s->BlendDesc.RenderTarget[i].LogicOp;
		}
	}
	else if (desc.type == crossplatform::RASTERIZER)
	{
		s->RasterDesc.FillMode					= toD3d12FillMode(desc.rasterizer.polygonMode);
		s->RasterDesc.CullMode					= toD3d12CullMode(desc.rasterizer.cullFaceMode);
		s->RasterDesc.FrontCounterClockwise		= desc.rasterizer.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE;
		s->RasterDesc.DepthBias					= 0.0f;
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
	dx12::Query *q=new dx12::Query(type);
	return q;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	,crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
#ifdef DO_DX12
	
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
	mCommandList = deviceContext.asD3D12Context();

#else

	UINT offset = 0;
	ID3D11Buffer *buf[10];
	UINT strides[10];
	UINT offsets[10];
	for (int i = 0; i<num_buffers; i++)
	{
		strides[i] = buffers[i]->stride;
		if (vertexSteps&&vertexSteps[i] >= 1)
			strides[i] *= vertexSteps[i];
		buf[i] = buffers[i]->AsD3D11Buffer();
		offsets[i] = 0;
	}
	deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(0,	// the first input slot for binding
		num_buffers,					// the number of buffers in the array
		buf,							// the array of vertex buffers
		strides,						// array of stride values, one for each buffer
		offsets);						// array of offset values, one for each buffer	

#endif // DO_DX12
};

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *vertexBuffer,int start_index)
{
	ID3D11Buffer *b=NULL;
	if(vertexBuffer)
		b=vertexBuffer->AsD3D11Buffer();
	UINT offset = vertexBuffer?(vertexBuffer->stride*start_index):0;
	deviceContext.asD3D11DeviceContext()->SOSetTargets(1,&b,&offset );
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	SIMUL_BREAK_ONCE("Nacho has to check this");

	PushRenderTargets(deviceContext);
	ID3D11RenderTargetView *rt[8];
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
		rt[i]=targs[i]->AsD3D11RenderTargetView();
	ID3D11DepthStencilView *d=NULL;
	if(depth)
		d=depth->AsD3D11DepthStencilView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(num,rt,d);

	int w=targs[0]->width, h = targs[0]->length;
	crossplatform::Viewport v[] = { { 0, 0, w, h, 0, 1.f }, { 0, 0, w, h, 0, 1.f }, { 0, 0, w, h, 0, 1.f } };
	SetViewports(deviceContext,num,v);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	PopRenderTargets(deviceContext);
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	D3D12_VIEWPORT viewports[8] = {};
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
	{
		viewports[i].Width		=(float)vps[i].w;
		viewports[i].Height		=(float)vps[i].h;
		viewports[i].TopLeftX	=(float)vps[i].x;
		viewports[i].TopLeftY	=(float)vps[i].y;
		viewports[i].MinDepth	=vps[i].znear;
		viewports[i].MaxDepth	=vps[i].zfar;
	}
	deviceContext.asD3D12Context()->RSSetViewports(num, viewports);
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	// TO-DO: Multiple viewports?
	return crossplatform::BaseFramebuffer::defaultTargetsAndViewport.viewport;
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetIndexBuffer(buffer->GetIndexBufferView());
	mCommandList = deviceContext.asD3D12Context();
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
	D3D_PRIMITIVE_TOPOLOGY T = toD3dTopology(t);
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetPrimitiveTopology(T);
	mStoredTopology = T;
	mCommandList = deviceContext.asD3D12Context();
}

void RenderPlatform::SetLayout(crossplatform::DeviceContext &deviceContext,crossplatform::Layout *l)
{
	if (l)
	{
		l->Apply(deviceContext);
	}
	else
	{
		deviceContext.asD3D11DeviceContext()->IASetInputLayout(NULL);
	}
	mCommandList = deviceContext.asD3D12Context();
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	// In DX12 there is only a few rendering states that we can set in "realtime" like stencil mask or blend value
	int a;
	a = s->type;
	mCommandList = deviceContext.asD3D12Context();
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	deviceContext.asD3D11DeviceContext()->ResolveSubresource(destination->AsD3D11Texture2D(),0,source->AsD3D11Texture2D(),0, dx12::RenderPlatform::ToDxgiFormat(destination->GetFormat()));
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext& deviceContext, bool error_checking)
{
	crossplatform::ContextState *cs = GetContextState(deviceContext);

	if (!cs||!cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}
	
	// NULL ptr here if we've not applied a valid shader..
	dx12::EffectPass *pass = static_cast<dx12::EffectPass*>(cs->currentEffectPass);
	Shader **sh = (dx12::Shader**)pass->shaders;

	// TODO: re-enable geometry shaders maybe
	if(sh[crossplatform::SHADERTYPE_GEOMETRY]||(sh[crossplatform::SHADERTYPE_VERTEX]!=nullptr&&sh[crossplatform::SHADERTYPE_PIXEL]==nullptr))
		return false;
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

		//std::cout << mLastFrame << ", (" << std::to_string(mCurIdx) << ")" << std::endl;

		// Reset the frame heaps (SRV_CBV_UAV and SAMPLER)
		mFrameHeap[mCurIdx].Reset();
		mFrameSamplerHeap[mCurIdx].Reset();
	}

	// Set the frame descriptor heaps
	ID3D12DescriptorHeap* currentHeaps[2] =
	{
		mFrameHeap[mCurIdx].GetHeap(),
		mFrameSamplerHeap[mCurIdx].GetHeap()
	};
	deviceContext.asD3D12Context()->SetDescriptorHeaps(2, currentHeaps);

	// CBV_SRV_UAV table
	if (pass->usesBuffers() || pass->usesTextures() || pass->usesSBs())
	{
		if(pass->IsCompute())
			deviceContext.asD3D12Context()->SetComputeRootDescriptorTable(pass->ResourceTableIndex(), mFrameHeap[mCurIdx].GpuHandle());
		else
			deviceContext.asD3D12Context()->SetGraphicsRootDescriptorTable(pass->ResourceTableIndex(), mFrameHeap[mCurIdx].GpuHandle());

	}
	// SAMPLER table
	if (pass->usesSamplers())
	{
		if(pass->IsCompute())
			deviceContext.asD3D12Context()->SetComputeRootDescriptorTable(pass->SamplerTableIndex(), mFrameSamplerHeap[mCurIdx].GpuHandle());
		else
			deviceContext.asD3D12Context()->SetGraphicsRootDescriptorTable(pass->SamplerTableIndex(), mFrameSamplerHeap[mCurIdx].GpuHandle());
	}

	// -----------------------------------------------------------------------------------------// 
	// NOTE: The order in which we apply the resources, MUST be the same order that we used		//
	// to create the RootSignature. So each descriptor will land in the required slot			//
	// this means that if we don't provide a resource or we provide too many resources we will	//
	// be steping on other resources potentially crashing or making undef behaviours.			//
	// -----------------------------------------------------------------------------------------//

	// Apply Samplers:	
	if (pass->usesSamplers())
	{
		auto dx12Effect = (dx12::Effect*)cs->currentEffect;
		auto sampMap = dx12Effect->GetSamplers();
		pass->SetSamplers(sampMap,&mFrameSamplerHeap[mCurIdx],m12Device);
	}
	
	// Apply Constant Buffers:
	if (!cs->buffersValid&&pass->usesBuffers())
	{
		pass->SetConstantBuffers(cs->applyBuffers, &mFrameHeap[mCurIdx],m12Device,deviceContext);
	}
	// Apply textures:
	if (!cs->textureAssignmentMapValid&&pass->usesTextures())
	{
		pass->SetTextures(cs->textureAssignmentMap, &mFrameHeap[mCurIdx], m12Device, deviceContext);
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
			//if(pass->UsesBufferSlot(i.first))
			dx12::Buffer* b = (dx12::Buffer*)(i.second);

			auto B=b->AsD3D11Buffer();
			//d3d11DeviceContext->IASetVertexBuffers( i.first, b->count,&B,nullptr,nullptr);
		}
		cs->vertexBuffersValid = true;
	}
	return true;
}

void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{

}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{

}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend/*=false*/)
{
	// If it was bound as a render target, we have to transition the resource so we can use it in the shaders
	dx12::Texture* dx12Texture = (dx12::Texture*)tex;
	if (tex->HasRenderTargets())
	{
		SIMUL_BREAK_ONCE("Check this");
		ResourceTransitionSimple(dx12Texture->AsD3D12Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	
	crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend);

	// Transition back to rt
	if (tex->HasRenderTargets())
	{
		SIMUL_BREAK_ONCE("Check this");
		ResourceTransitionSimple(dx12Texture->AsD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	mCommandList = deviceContext.asD3D12Context();
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	mCommandList = deviceContext.asD3D12Context();

	SetTopology(deviceContext,simul::crossplatform::TRIANGLESTRIP);
	ClearIA(deviceContext);
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

namespace simul
{
	namespace dx12
	{
		struct RTState
		{
			RTState():depthSurface(NULL),numViewports(0)
			{
				memset(renderTargets,0,sizeof(renderTargets));
				memset(viewports,0,sizeof(viewports));
			}
			ID3D11RenderTargetView*				renderTargets[16];
			ID3D11DepthStencilView*				depthSurface;
			D3D11_VIEWPORT						viewports[16];
			unsigned							numViewports;
		};
	}
}

void RenderPlatform::PushRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_BREAK_ONCE("Check this");
	/*
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	RTState *state=new RTState;
	pContext->RSGetViewports(&state->numViewports,NULL);
	if(state->numViewports>0)
		pContext->RSGetViewports(&state->numViewports,state->viewports);
	pContext->OMGetRenderTargets(	state->numViewports,
									state->renderTargets,
									&state->depthSurface);
	storedRTStates.push_back(state);
	*/
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_BREAK_ONCE("Check this");
	/*
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	RTState *state=storedRTStates.back();
	pContext->OMSetRenderTargets(	state->numViewports,
									state->renderTargets,
									state->depthSurface);
	for(int i=0;i<(int)state->numViewports;i++)
	{
		SAFE_RELEASE(state->renderTargets[i]);
	}
	SAFE_RELEASE(state->depthSurface);
	if(state->numViewports>0)
		pContext->RSSetViewports(state->numViewports,state->viewports);
	delete state;
	storedRTStates.pop_back();
	*/
}

crossplatform::Shader *RenderPlatform::CreateShader()
{
	Shader *S = new Shader();
	return S;
}

#pragma optimize("",on)