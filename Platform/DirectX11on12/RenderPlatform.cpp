#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX11on12/RenderPlatform.h"
#include "Simul/Platform/DirectX11on12/Material.h"
#include "Simul/Platform/DirectX11on12/Mesh.h"
#include "Simul/Platform/DirectX11on12/Texture.h"
#include "Simul/Platform/DirectX11on12/ESRAMTexture.h"
#include "Simul/Platform/DirectX11on12/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11on12/Light.h"
#include "Simul/Platform/DirectX11on12/Effect.h"
#include "Simul/Platform/DirectX11on12/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11on12/Buffer.h"
#include "Simul/Platform/DirectX11on12/Layout.h"
#include "Simul/Platform/DirectX11on12/MacrosDX1x.h"
#include "Simul/Platform/DirectX11on12/SaveTextureDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11on12/CompileShaderDX1x.h"
#include "Simul/Platform/DirectX11on12/ConstantBuffer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "D3dx11effect.h"
#ifdef _XBOX_ONE
#include "Simul/Platform/DirectX11/ESRAMManager.h"
#include <D3Dcompiler_x.h>
#else
#include <D3Dcompiler.h>
#endif
#include <algorithm>
#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif

#include "Simul/Platform/DirectX11on12/Utilities.h"

#include "d3d12.h"

using namespace simul;
using namespace dx11on12;

void RenderPlatform::StoredState::Clear()
{
	m_StencilRefStored11			=0;
	m_SampleMaskStored11			=0;
	m_indexOffset					=0;
	m_indexFormatStored11			=DXGI_FORMAT_UNKNOWN;
	m_previousTopology				=D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_pDepthStencilStateStored11	=NULL;
	m_pRasterizerStateStored11		=NULL;
	m_pBlendStateStored11			=NULL;
	pIndexBufferStored11			=NULL;
	m_previousInputLayout			=NULL;
	pVertexShader					=NULL;
	pPixelShader					=NULL;
	pHullShader						=NULL;
	pDomainShader					=NULL;
	pGeometryShader					=NULL;
	pComputeShader					=NULL;

	for(int i=0;i<4;i++)
		m_BlendFactorStored11[i];
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
		m_pSamplerStateStored11[i]=NULL;
		m_pVertexSamplerStateStored11[i]=NULL;
		m_pComputeSamplerStateStored11[i]=NULL;
		m_pGeometrySamplerStateStored11[i]=NULL;
	}
	for(int i=0;i<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;i++)
	{
		m_pCSConstantBuffers[i]=NULL;
		m_pGSConstantBuffers[i]=NULL;
		m_pPSConstantBuffers[i]=NULL;
		m_pVSConstantBuffers[i]=NULL;
		m_pHSConstantBuffers[i]=NULL;
		m_pDSConstantBuffers[i]=NULL;
	}
	for (int i = 0; i<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		m_pShaderResourceViews[i]=NULL;
		m_pCSShaderResourceViews[i]=NULL;
	}
	for (int i = 0; i<D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		m_pUnorderedAccessViews[i] = NULL;
	}
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
		m_pVertexBuffersStored11[i]=NULL;
		m_VertexStrides[i]=0;
		m_VertexOffsets[i]=0;
	}
	numPixelClassInstances=0;
	for(int i=0;i<16;i++)
	{
		m_pPixelClassInstances[i]=NULL;
	}
	numVertexClassInstances=0;
	for(int i=0;i<16;i++)
	{
		m_pVertexClassInstances[i]=NULL;
	}
}

RenderPlatform::RenderPlatform():
	device(NULL),
	storedStateCursor(0),
	pUserDefinedAnnotation(NULL),
	m11on12Device(NULL),
	m12CommandList(NULL),
	m12Device(NULL)
{
	storedStates.resize(4);
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

ID3D11On12Device* RenderPlatform::AsD3D11On12Device()
{
	return m11on12Device;
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return m12CommandList;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return m12Device;
}

void RenderPlatform::ResourceTransitionSimple(ID3D12Resource * res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition
	(
		res,before,after
	);
	m12CommandList->ResourceBarrier(1, &barrier);
}

void RenderPlatform::PushToReleaseManager(ID3D12Resource* res, std::string dName)
{
	if (!res)
		return;
	// Don't add duplicates, this operation can be potentially slow if we have tons of resources
	for (unsigned int i = 0; i < mResourceBin.size(); i++)
	{
		if (res == mResourceBin[i].second.second)
			return;
	}
	mResourceBin.push_back(std::pair<int, std::pair<std::string,ID3D12Resource*>>
	(
		0, 
		std::pair<std::string, ID3D12Resource*>
		(
			dName,
			res
		)
	));
}

void RenderPlatform::PushToReleaseManager(ID3D12DescriptorHeap* res, std::string dName)
{
	if (!res)
		return;
	// Don't add duplicates, this operation can be potentially slow if we have tons of resources
	for (unsigned int i = 0; i < mHeapBin.size(); i++)
	{
		if (res == mHeapBin[i].second.second)
			return;
	}
	mHeapBin.push_back(std::pair<int, std::pair<std::string, ID3D12DescriptorHeap*>>
	(
		0, 
		std::pair<std::string, ID3D12DescriptorHeap*>
		(
			dName,
			res
		)
	));
}

void simul::dx11on12::RenderPlatform::ClearIA(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetVertexBuffers(0, 1, nullptr); // Only 1? 
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetIndexBuffer(nullptr);
}

#ifdef _XBOX_ONE
ESRAMManager *eSRAMManager=NULL;
#endif
void RenderPlatform::RestoreDeviceObjects(void* d)
{
	mCurInputLayout = nullptr;

	// DX11on12 objects
	// 0: device
	// 1: dx12 device
	// 2: commandList
	// 3: dx11on12 device
	// 4: dx12 command queue
	void** pData = (void**)d;

	if (m12Device != pData[1])
	{
		m12Device = (ID3D12Device*)pData[1];
	}
	if (m12CommandList != pData[2])
	{
		m12CommandList = (ID3D12GraphicsCommandList*)pData[2];
	}
	if (m11on12Device != pData[3])
	{
		m11on12Device = (ID3D11On12Device*)pData[3];
	}
	if (m12Queue != pData[4])
	{
		m12Queue = (ID3D12CommandQueue*)pData[4];
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

	mComputeFence.Restore(this);

	// Create dummy textures
	mDummy2D = CreateTexture("Dummy2D");
	mDummy3D = CreateTexture("Dummy3D");
	mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);
	mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM, true);

	crossplatform::RenderPlatform::RestoreDeviceObjects(pData[0]);
	RecompileShaders();

	// DX11
	if(device== pData[0])
		return;

	device=(ID3D11Device*)pData[0];
	
	ID3D11DeviceContext *pImmediateContext;
	AsD3D11Device()->GetImmediateContext(&pImmediateContext);
	immediateContext.platform_context=pImmediateContext;
	immediateContext.renderPlatform=this;

#ifdef _XBOX_ONE
	delete eSRAMManager;
	eSRAMManager=new ESRAMManager(device);
#endif
	
	
#ifdef SIMUL_WIN8_SDK
	{
		SAFE_RELEASE(pUserDefinedAnnotation);
		IUnknown *unknown=(IUnknown *)pImmediateContext;
#ifdef _XBOX_ONE
		V_CHECK(unknown->QueryInterface( __uuidof(pUserDefinedAnnotation), reinterpret_cast<void**>(&pUserDefinedAnnotation) ));
#else
		V_CHECK(unknown->QueryInterface(IID_PPV_ARGS(&pUserDefinedAnnotation)));
#endif
}
#endif
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
		dx11on12::Material *mat=(dx11on12::Material*)(*i);
		delete mat;
	}
	materials.clear();
	SAFE_DELETE(debugEffect);
	ID3D11DeviceContext* c=immediateContext.asD3D11DeviceContext();
	SAFE_RELEASE(c);
	immediateContext.platform_context=NULL;
	device=NULL;
}

void RenderPlatform::RecompileShaders()
{
	if(!m12Device)
		return;
	crossplatform::RenderPlatform::RecompileShaders();
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11on12::Material *mat=(dx11on12::Material*)(*i);
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
	//simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	//SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_TEST_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_TEST_DEPTH_LESS_EQUAL);

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
				if (remainRefs)
					SIMUL_CERR << mResourceBin[i].second.first << " is still referenced( " << remainRefs << " )" << std::endl;
				mResourceBin.erase(mResourceBin.begin() + i);
			}
		}
	}
	if (!mHeapBin.empty())
	{
		for (int i = mHeapBin.size() - 1; i >= 0; i--)
		{
			mHeapBin[i].first++;
			if (mHeapBin[i].first >= kMaxAge)
			{
				int remainRefs = 0;
				if (mHeapBin[i].second.second)
					remainRefs = mHeapBin[i].second.second->Release();
				if(remainRefs)
					SIMUL_CERR << mHeapBin[i].second.first << " is still referenced( " << remainRefs << " )" << std::endl;
				mHeapBin.erase(mHeapBin.begin() + i);
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
		Read comment of dx11on12::getTexel about copying resources in DX12
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

bool firstDisp = true;
void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
/*
	if (!firstDisp)
		mComputeFence.WaitGPU(this);
*/
	ApplyContextState(deviceContext);
	deviceContext.renderPlatform->AsD3D12CommandList()->Dispatch(w, l, d);
//	mComputeFence.Signal(this);
	firstDisp = false;
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	
}

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	ClearIA(deviceContext);
	ApplyContextState(deviceContext);
	// In case we didn't call set topology, we should really just call SetTopology() <----------------- 
	//deviceContext.renderPlatform->AsD3D12CommandList()->IASetPrimitiveTopology(mStoredTopology);
	deviceContext.renderPlatform->AsD3D12CommandList()->DrawInstanced(num_verts,1,start_vert,0);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	ClearIA(deviceContext);
	ApplyContextState(deviceContext);
	// In case we didn't call set topology, we should really just call SetTopology() <----------------- 
	//deviceContext.renderPlatform->AsD3D12CommandList()->IASetPrimitiveTopology(mStoredTopology);
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
	dx11on12::Material *mat=new dx11on12::Material;
	mat->SetEffect(solidEffect);
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new dx11on12::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new dx11on12::Light();
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
	tex=new dx11on12::Texture();
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
	return new dx11on12::Framebuffer(name);
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
	}
}
static D3D12_FILTER ToD3D12Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	switch (f)
	{
	case simul::crossplatform::SamplerStateDesc::POINT:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
		break;
	case simul::crossplatform::SamplerStateDesc::LINEAR:
		return D3D12_FILTER_ANISOTROPIC;
		break;
	case simul::crossplatform::SamplerStateDesc::ANISOTROPIC:
		return D3D12_FILTER_ANISOTROPIC;
		break;
	}
}

static D3D11_TEXTURE_ADDRESS_MODE toD3d11TextureAddressMode(crossplatform::SamplerStateDesc::Wrapping f)
{
	if(f==crossplatform::SamplerStateDesc::CLAMP)
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	if(f==crossplatform::SamplerStateDesc::WRAP)
		return D3D11_TEXTURE_ADDRESS_WRAP;
	if(f==crossplatform::SamplerStateDesc::MIRROR)
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	return D3D11_TEXTURE_ADDRESS_WRAP;
}

D3D11_FILTER toD3d11Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	if(f==crossplatform::SamplerStateDesc::LINEAR)
		return D3D11_FILTER_ANISOTROPIC;
	return D3D11_FILTER_MIN_MAG_MIP_POINT;
}

crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	dx11on12::SamplerState *s = new dx11on12::SamplerState(d);

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
	crossplatform::Effect *e= new dx11on12::Effect();
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx11on12::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx11on12::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx11on12::Buffer();
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

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc)
{
	dx11on12::Layout *l = new dx11on12::Layout();

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
		break;
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
	default:
		SIMUL_BREAK("Undefined polygon mode");
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
		break;
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
		break;
	}
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx11on12::RenderState *s=new dx11on12::RenderState();
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
	dx11on12::Query *q=new dx11on12::Query(type);
	return q;
}

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	,crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
#ifdef DO_DX12
	
	if (num_buffers > 1)
	{
		SIMUL_BREAK("Nacho has to work to do here!!");
	}
	auto pBuffer = (dx11on12::Buffer*)buffers[0];
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetVertexBuffers
	(
		0,
		num_buffers,
		pBuffer->GetVertexBufferView()
	);

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
	deviceContext.renderPlatform->AsD3D12CommandList()->RSSetViewports(num, viewports);
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	// TO-DO: Multiple viewports?
	return crossplatform::BaseFramebuffer::defaultTargetsAndViewport.viewport;
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
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
	D3D_PRIMITIVE_TOPOLOGY T = toD3dTopology(t);
	deviceContext.renderPlatform->AsD3D12CommandList()->IASetPrimitiveTopology(T);
	mStoredTopology = T;
}

void RenderPlatform::SetLayout(crossplatform::DeviceContext &deviceContext,crossplatform::Layout *l)
{
	if(l)
		l->Apply(deviceContext);
	else
	{
		deviceContext.asD3D11DeviceContext()->IASetInputLayout(NULL);
	}
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	// In DX12 there is only a few rendering states that we can set in "realtime" like stencil mask or blend value
	int a;
	a = s->type;
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	deviceContext.asD3D11DeviceContext()->ResolveSubresource(destination->AsD3D11Texture2D(),0,source->AsD3D11Texture2D(),0, dx11on12::RenderPlatform::ToDxgiFormat(destination->GetFormat()));
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	dx11on12::SaveTexture(device,texture->AsD3D11Texture2D(),lFileNameUtf8);
}

bool SortTextureMap(const std::pair<int, crossplatform::TextureAssignment>& l, const std::pair<int, crossplatform::TextureAssignment>& r)
{
	return l.first < r.first;
}

bool SortCbMap(const std::pair<int, crossplatform::ConstantBufferBase*>& l, const std::pair<int, crossplatform::ConstantBufferBase*>& r)
{
	return l.first < r.first;
}

bool SortSbMap(const std::pair<int, crossplatform::PlatformStructuredBuffer*>& l, const std::pair<int, crossplatform::PlatformStructuredBuffer*>& r)
{
	return l.first < r.first;
}

bool SortSamplerMap(const std::pair<crossplatform::SamplerState*, int>& l, const std::pair<crossplatform::SamplerState*, int>& r)
{
	return l.second < r.second;
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext &deviceContext, bool error_checking)
{
	crossplatform::ContextState *cs = GetContextState(deviceContext);
	if (!cs||!cs->currentEffectPass)
	{
		SIMUL_BREAK("No valid shader pass in ApplyContextState");
		return false;
	}
	
	// NULL ptr here if we've not applied a valid shader..
	dx11on12::EffectPass *pass = static_cast<dx11on12::EffectPass*>(cs->currentEffectPass);
	Shader **sh = (dx11on12::Shader**)pass->shaders;

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
		pass->Apply(deviceContext, false);
		cs->effectPassValid = true;
	}

	// We will only set the tables once per frame
	if (mLastFrame != deviceContext.frame_number)
	{
		mLastFrame = deviceContext.frame_number;

		// Reset the frame heaps (SRV_CBV_UAV and SAMPLER)
		mFrameHeap[deviceContext.cur_backbuffer].Reset();
		mFrameSamplerHeap[deviceContext.cur_backbuffer].Reset();

		// Set the frame descriptor heaps
		ID3D12DescriptorHeap* currentHeaps[2] = 
		{
			mFrameHeap[deviceContext.cur_backbuffer].GetHeap(),
			mFrameSamplerHeap[deviceContext.cur_backbuffer].GetHeap()
		};
		m12CommandList->SetDescriptorHeaps(2, currentHeaps);
	}
	// RootIndex0: CBV_SRV_UAV table
	if (pass->usesBuffers() || pass->usesTextures() || pass->usesSBs())
	{
		if(pass->mIsCompute)
			m12CommandList->SetComputeRootDescriptorTable(0, mFrameHeap[deviceContext.cur_backbuffer].GpuHandle());
		else
			m12CommandList->SetGraphicsRootDescriptorTable(0, mFrameHeap[deviceContext.cur_backbuffer].GpuHandle());

	}
	// RootIndex1: SAMPLER table
	if (pass->usesSamplers())
	{
		if(pass->mIsCompute)
			m12CommandList->SetComputeRootDescriptorTable(1, mFrameSamplerHeap[deviceContext.cur_backbuffer].GpuHandle());
		else
			m12CommandList->SetGraphicsRootDescriptorTable(1, mFrameSamplerHeap[deviceContext.cur_backbuffer].GpuHandle());
	}

	// -----------------------------------------------------------------------------------------// 
	// NOTE: The order in which we apply the resources, MUST be the same order that we used		//
	// to create the RootSignature. So each descriptor will land in the required slot			//
	// -----------------------------------------------------------------------------------------//

	// Apply Samplers:	
	if (pass->usesSamplers())
	{
		auto dx12Effect = (dx11on12::Effect*)cs->currentEffect;
		auto sampMap = dx12Effect->GetSamplers();

		// Build a sorted vector of <SamplerState*, slot>
		std::vector<std::pair<crossplatform::SamplerState*,int>> samplerAssignOrdered;
		for (auto i = sampMap.begin(); i != sampMap.end(); i++)
		{
			samplerAssignOrdered.push_back(std::make_pair(i->first, i->second));
		}
		std::sort(samplerAssignOrdered.begin(), samplerAssignOrdered.end(), SortSamplerMap);

		for each(auto s in samplerAssignOrdered)
		{
			if (pass->usesSamplerSlot(s.second))
			{
				auto dx12Sampler = (dx11on12::SamplerState*)s.first;

				// Destination
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameSamplerHeap[deviceContext.cur_backbuffer].CpuHandle();
				mFrameSamplerHeap[deviceContext.cur_backbuffer].Offset();

				// Source
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = *dx12Sampler->AsD3D12SamplerState();

				// Copy!
				m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
	}
	
	// Apply Constant Buffers:
	if (!cs->buffersValid&&pass->usesBuffers())
	{
		// Build a sorted vector of <slot, ConstantBufferBase*>
		std::vector<std::pair<int, crossplatform::ConstantBufferBase*>> cbAssignOrdered;
		for (auto i = cs->applyBuffers.begin(); i != cs->applyBuffers.end(); i++)
		{
			cbAssignOrdered.push_back(std::make_pair(i->first, i->second));
		}
		std::sort(cbAssignOrdered.begin(), cbAssignOrdered.end(), SortCbMap);

		cs->bufferSlots = 0;
		for (auto i = cbAssignOrdered.begin(); i != cbAssignOrdered.end(); i++)
		{
			int slot = i->first;
			if (!pass->usesBufferSlot(slot))
			{
				SIMUL_CERR_ONCE << "The constant buffer at slot: " << slot << " , is applied but the pass does not use it, ignoring it." << std::endl;
				continue;
			}
			if (slot != i->second->GetIndex())
			{
				SIMUL_BREAK_ONCE("This buffer assigned to the wrong index.");
			}
			i->second->GetPlatformConstantBuffer()->ActualApply(deviceContext, pass, i->second->GetIndex());

			// Destination
			D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameHeap[deviceContext.cur_backbuffer].CpuHandle();
			mFrameHeap[deviceContext.cur_backbuffer].Offset();

			// Source
			auto dx12cb = (dx11on12::PlatformConstantBuffer*)i->second->GetPlatformConstantBuffer();
			D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = dx12cb->AsD3D12ConstantBuffer();

			// Copy!
			m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			if (error_checking&&pass->usesBufferSlot(slot))
				cs->bufferSlots |= (1 << slot);

		}
		if (error_checking)
		{
			unsigned required_buffer_slots = pass->GetConstantBufferSlots();
			if ((cs->bufferSlots&required_buffer_slots) != required_buffer_slots)
			{
				SIMUL_BREAK_ONCE("Not all constant buffers are assigned.");
				unsigned missing_slots = required_buffer_slots&(~cs->bufferSlots);
				for (unsigned i = 0; i<32; i++)
				{
					unsigned slot = 1 << i;
					if (slot&missing_slots)
						SIMUL_CERR << "Slot " << i << " was not set." << std::endl;
				}
			}
		}
	}
	// Apply textures:
	if (!cs->textureAssignmentMapValid&&pass->usesTextures())
	{
		cs->textureSlots = 0;
		cs->rwTextureSlots = 0;

		// !!!!!!!!!!!!!!!!! Why? If we where using the effect::Unbind texture properly this shouldnt be needed?

		// if I uncoment this text rendering will work bad

		// The thing is, that we need for each draw call to copy the new batch of descriptors, it should be possible however
		// to detect if we are using over and over the same PSO and RootSignature, then, we will just left that pso bound 
		// and also the descriptors

		//cs->textureAssignmentMapValid = true;
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		// Build a sorted vector of <slot, textureassignment>
		std::vector<std::pair<int, crossplatform::TextureAssignment>> texAssignOrdered;
		for (auto i = cs->textureAssignmentMap.begin(); i != cs->textureAssignmentMap.end(); i++)
		{
			texAssignOrdered.push_back(std::make_pair(i->first,i->second));
		}
		std::sort(texAssignOrdered.begin(), texAssignOrdered.end(), SortTextureMap);

		// Check for Textures (SRV)
		for (auto i = texAssignOrdered.begin(); i != texAssignOrdered.end(); i++)
		{
			int slot = i->first;
			if (slot < 0)
				continue;
			if (!pass->usesTextureSlot(slot))
			{
				SIMUL_CERR_ONCE << "The texture at slot: " << slot << " , is applied but the pass does not use it, ignoring it." << std::endl;
				continue;
			}

			crossplatform::TextureAssignment &ta = i->second;

			// If we request a NULL texture, send a dummy!
			if (!ta.texture || !ta.texture->IsValid())
			{
				if (ta.dimensions == 2)
					ta.texture = mDummy2D;
				else if (ta.dimensions == 3)
					ta.texture = mDummy3D;
				else
					SIMUL_BREAK("Only have dummy for 2D and 3D");
			}

			Shader **sh = (Shader**)pass->shaders;
			if (!ta.uav)
			{
				SIMUL_ASSERT_WARN_ONCE(slot<1000, "Bad slot number");

				UINT curFrameIndex = deviceContext.cur_backbuffer;

				// Destination
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameHeap[curFrameIndex].CpuHandle();
				mFrameHeap[curFrameIndex].Offset();

				// Source
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = *ta.texture->AsD3D12ShaderResourceView(true,ta.resourceType);

				// Copy!
				m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cs->textureSlots |= (1 << slot);
			}
		}
		// Check for RWTextures (UAV)
		for (auto i = texAssignOrdered.begin(); i != texAssignOrdered.end(); i++)
		{
			int slot = i->first;
			if (slot<0)
				continue;
			if (!pass->usesTextureSlot(slot))
			{
				SIMUL_CERR_ONCE << "The texture at slot: " << slot << " , is applied but the pass does not use it, ignoring it." << std::endl;
				continue;
			}

			crossplatform::TextureAssignment &ta = i->second;

			// If we request a NULL texture, send a dummy!
			if (!ta.texture || !ta.texture->IsValid())
			{
				if (ta.dimensions == 2)
					ta.texture = mDummy2D;
				else if (ta.dimensions == 3)
					ta.texture = mDummy3D;
				else
					SIMUL_BREAK("Only have dummy for 2D and 3D");
			}

			Shader **sh = (Shader**)pass->shaders;
			if (ta.uav && sh[crossplatform::SHADERTYPE_COMPUTE] && sh[crossplatform::SHADERTYPE_COMPUTE]->usesRwTextureSlot(slot - 1000))
			{
				SIMUL_ASSERT_WARN_ONCE(slot >= 1000, "Bad slot number");

				UINT curFrameIndex = deviceContext.cur_backbuffer;

				// Destination
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameHeap[curFrameIndex].CpuHandle();
				mFrameHeap[curFrameIndex].Offset();

				// Source
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = *ta.texture->AsD3D12UnorderedAccessView(ta.index, ta.mip);

				// Copy!
				m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cs->rwTextureSlots |= (1 << (slot - 1000));
			}
		}
		// Now verify that ALL resource are set:
		if (error_checking)
		{
			unsigned required_slots = pass->GetTextureSlots();
			if ((cs->textureSlots&required_slots) != required_slots)
			{
				static int count = 10;
				count--;
				if (count>0)
				{
					SIMUL_CERR << "Not all texture slots are assigned:" << std::endl;
					unsigned missing_slots = required_slots&(~cs->textureSlots);
					for (unsigned i = 0; i<32; i++)
					{
						unsigned slot = 1 << i;
						if (slot&missing_slots)
						{
							std::string name;
							if (cs->currentEffect)
								name = cs->currentEffect->GetTextureForSlot(i);
							SIMUL_CERR << "\tSlot " << i << ": " << name.c_str() << ", was not set." << std::endl;
							missing_slots = missing_slots&(~slot);
							if (!missing_slots)
								break;
						}
					}
					//SIMUL_BREAK_ONCE("Many API's require all used textures to have valid data.");
				}
			}
			unsigned required_rw_slots = pass->GetRwTextureSlots();
			if ((cs->rwTextureSlots&required_rw_slots) != required_rw_slots)
			{
				static int count = 10;
				count--;
				if (count>0)
				{
					SIMUL_BREAK_ONCE("Not all rw texture slots are assigned.");
					required_rw_slots = required_rw_slots&(~cs->rwTextureSlots);
					for (unsigned i = 0; i<32; i++)
					{
						unsigned slot = 1 << i;
						if (slot&required_rw_slots)
						{
							std::string name;
							if (cs->currentEffect)
								name = cs->currentEffect->GetTextureForSlot(1000 + i);
							SIMUL_CERR << "RW Slot " << i << " was not set (" << name.c_str() << ")." << std::endl;
						}
					}
				}
			}
		}
	}

	// Apply StructuredBuffers:
	cs->textureSlotsForSB = 0;
	cs->rwTextureSlotsForSB = 0;
	if (pass->usesSBs())
	{
		// Build a sorted vector of <slot, PlatformStructuredBuffer*>
		std::vector<std::pair<int, crossplatform::PlatformStructuredBuffer*>> sbAssignOrdered;
		for (auto i = cs->applyStructuredBuffers.begin(); i != cs->applyStructuredBuffers.end(); i++)
		{
			sbAssignOrdered.push_back(std::make_pair(i->first, i->second));
		}
		std::sort(sbAssignOrdered.begin(), sbAssignOrdered.end(), SortSbMap);

		// StructuredBuffer
		for (auto i = sbAssignOrdered.begin(); i != sbAssignOrdered.end(); i++)
		{
			int slot = i->first;
			if (!pass->usesTextureSlotForSB(slot))
			{
				SIMUL_CERR_ONCE << "The structured buffer at slot: " << slot << " , is applied but the pass does not use it, ignoring it." << std::endl;
				continue;
			}

			if (slot<1000)
			{
				i->second->ActualApply(deviceContext, pass, slot);
				auto sb = (dx11on12::PlatformStructuredBuffer*)i->second;
				UINT curFrameIndex = deviceContext.cur_backbuffer;

				// Destination
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameHeap[curFrameIndex].CpuHandle();
				mFrameHeap[curFrameIndex].Offset();

				// Source
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = *sb->AsD3D12ShaderResourceView();

				// Copy!
				m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cs->textureSlotsForSB |= (1 << slot);
			}
		}
		// RWStructuredBuffer
		for (auto i = cs->applyStructuredBuffers.begin(); i != cs->applyStructuredBuffers.end(); i++)
		{
			int slot = i->first;
			if (!pass->usesTextureSlotForSB(slot))
			{
				SIMUL_CERR_ONCE << "The structured buffer at slot: " << slot << " , is applied but the pass does not use it, ignoring it." << std::endl;
				continue;
			}

			if (slot >= 1000)
			{
				i->second->ActualApply(deviceContext, pass, slot);
				auto sb = (dx11on12::PlatformStructuredBuffer*)i->second;
				UINT curFrameIndex = deviceContext.cur_backbuffer;

				// Destination
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = mFrameHeap[curFrameIndex].CpuHandle();
				mFrameHeap[curFrameIndex].Offset();

				// Source
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = *sb->AsD3D12UnorderedAccessView();

				// Copy!
				m12Device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cs->rwTextureSlotsForSB |= (1 << (slot - 1000));
			}
		}

		if (error_checking)
		{
			unsigned required_sb_slots = pass->GetStructuredBufferSlots();
			if ((cs->textureSlotsForSB&required_sb_slots) != required_sb_slots)
			{
				SIMUL_BREAK_ONCE("Not all texture slots are assigned.");
			}
			unsigned required_rw_sb_slots = pass->GetRwStructuredBufferSlots();
			if ((cs->rwTextureSlotsForSB&required_rw_sb_slots) != required_rw_sb_slots)
			{
				SIMUL_BREAK_ONCE("Not all texture slots are assigned.");
			}
		}
	}

	if (!cs->streamoutTargetsValid)
	{
		for (auto i = cs->streamoutTargets.begin(); i != cs->streamoutTargets.end(); i++)
		{
			int slot = i->first;
			dx11on12::Buffer *vertexBuffer = (dx11on12::Buffer*)i->second;
			if (!vertexBuffer)
				continue;

			ID3D11Buffer *b = vertexBuffer->AsD3D11Buffer();
			if (!b)
				continue;
		}
		cs->streamoutTargetsValid = true;
	}

	if (!cs->vertexBuffersValid)
	{
		for (auto i : cs->applyVertexBuffers)
		{
			//if(pass->UsesBufferSlot(i.first))
			dx11on12::Buffer* b = (dx11on12::Buffer*)(i.second);

			auto B=b->AsD3D11Buffer();
			//d3d11DeviceContext->IASetVertexBuffers( i.first, b->count,&B,nullptr,nullptr);
		}
		cs->vertexBuffersValid = true;
	}

	return true;
}

#pragma optimize("",off)
void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{

}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{

}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend/*=false*/)
{
	// If it was bound as a render target, we have to transition the resource so we can use it in the shaders
	dx11on12::Texture* dx12Texture;
	/*
	if (tex->HasRenderTargets())
	{
		dx12Texture = (dx11on12::Texture*)tex;
		ResourceTransitionSimple(dx12Texture->AsD3D12Resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	*/
	crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend);

	// Transition back to rt
	if (tex->HasRenderTargets())
	{
		ResourceTransitionSimple(dx12Texture->AsD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
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
	namespace dx11on12
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
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	RTState *state=new RTState;
	pContext->RSGetViewports(&state->numViewports,NULL);
	if(state->numViewports>0)
		pContext->RSGetViewports(&state->numViewports,state->viewports);
	pContext->OMGetRenderTargets(	state->numViewports,
									state->renderTargets,
									&state->depthSurface);
	storedRTStates.push_back(state);
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &deviceContext)
{
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
}

crossplatform::Shader *RenderPlatform::CreateShader()
{
	Shader *S = new Shader();
	return S;
}