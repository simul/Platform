#define NOMINMAX
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/DirectX11/Material.h"
#include "Platform/DirectX11/Mesh.h"
#include "Platform/DirectX11/Texture.h"
#include "Platform/DirectX11/ESRAMTexture.h"
#include "Platform/DirectX11/FramebufferDX1x.h"
#include "Platform/DirectX11/Light.h"
#include "Platform/DirectX11/Effect.h"
#include "Platform/DirectX11/CreateEffectDX1x.h"
#include "Platform/DirectX11/Buffer.h"
#include "Platform/DirectX11/Layout.h"
#include "Platform/DirectX11/MacrosDX1x.h"
#include "Platform/DirectX11/SaveTextureDX1x.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/DirectX11/CompileShaderDX1x.h"
#include "Platform/DirectX11/PlatformStructuredBuffer.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/CrossPlatform/Camera.h"

#if !PLATFORM_D3D11_SFX
#include "D3dx11effect.h"
#endif
#include "DisplaySurface.h"

#ifdef _XBOX_ONE
#include "Platform/DirectX11/ESRAMManager.h"
#include <D3Dcompiler_x.h>
#include <d3d11_x.h>
#else
#include <D3Dcompiler.h>
#endif

#include <algorithm>
#ifdef USE_PIX
#include <pix.h>
#endif

#include "Platform/DirectX11/Utilities.h"
#pragma optimize("",off)

using namespace simul;
using namespace dx11;

struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t box_vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

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
		m_pPSShaderResourceViews[i]=NULL;
		m_pCSShaderResourceViews[i]=NULL;
		m_pVSShaderResourceViews[i]=NULL;
		m_pHSShaderResourceViews[i]=NULL;
		m_pGSShaderResourceViews[i]=NULL;
		m_pDSShaderResourceViews[i]=NULL;
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

RenderPlatform::RenderPlatform()
	:device(NULL)
	,storedStateCursor(0)
	,pUserDefinedAnnotation(NULL)
	,fence(0)
{
	storedStates.resize(4);
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

ID3D11Device *RenderPlatform::AsD3D11Device()
{
/*	if(!device)
		return 0;
	((ID3D11Device*)device)->AddRef();
	int ref=((ID3D11Device*)device)->Release();
	std::cerr<<ref<<" ";*/
	return device;
}

#ifdef _XBOX_ONE
ESRAMManager *eSRAMManager=NULL;
#endif
void RenderPlatform::RestoreDeviceObjects(void *d)
{
	if(device==d)
		return;
	fence=0;
	device=(ID3D11Device*)d;
	crossplatform::RenderPlatform::RestoreDeviceObjects(d);
	
	ID3D11DeviceContext *pImmediateContext;
	AsD3D11Device()->GetImmediateContext(&pImmediateContext);
	immediateContext.platform_context=pImmediateContext;
	immediateContext.renderPlatform=this;
#ifdef _XBOX_ONE
	delete eSRAMManager;
	eSRAMManager=new ESRAMManager(device);
#endif
	RecompileShaders();
	RecompileShaders();
	
	{
		SAFE_RELEASE(pUserDefinedAnnotation);
		IUnknown *unknown=(IUnknown *)pImmediateContext;
#ifdef _XBOX_ONE
		V_CHECK(unknown->QueryInterface(__uuidof(pUserDefinedAnnotation), reinterpret_cast<void**>(&pUserDefinedAnnotation)));
#else
		V_CHECK(unknown->QueryInterface(IID_PPV_ARGS(&pUserDefinedAnnotation)));
#endif
	}
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
	SAFE_DELETE(debugEffect);
	ID3D11DeviceContext* c=immediateContext.asD3D11DeviceContext();
	SAFE_RELEASE(c);
	immediateContext.platform_context=NULL;
	device=NULL;
}

void RenderPlatform::RecompileShaders()
{
	if(!device)
		return;
	crossplatform::RenderPlatform::RecompileShaders();
}

void RenderPlatform::BeginEvent	(crossplatform::DeviceContext &,const char *name)
{
	static std::unordered_map<const char*,const wchar_t*> name_map;
	auto n=name_map.find(name);
	const wchar_t *wstr_name=nullptr;
	if(n==name_map.end())
	{
		static std::vector<std::wstring> wstrs;
		std::wstring wn=base::Utf8ToWString(name);
		// make sure the string is permanently stored:
		wstrs.push_back(wn);
		// and this pointer will remain valid until shutdown.
		wstr_name=wstrs.back().c_str();
		name_map[name]=wstr_name;
	}
	else
		wstr_name=n->second;
	if(pUserDefinedAnnotation&&wstr_name)
		pUserDefinedAnnotation->BeginEvent(wstr_name);
#ifdef USE_PIX
	PIXBeginEvent( 0, wstr_name );
#endif
}

void RenderPlatform::EndEvent			(crossplatform::DeviceContext &)
{
#ifdef SIMUL_WIN8_SDK
	if(pUserDefinedAnnotation)
		pUserDefinedAnnotation->EndEvent();
#endif
#ifdef USE_PIX
	PIXEndEvent();
#endif
}

void RenderPlatform::BeginFrame(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::BeginFrame(deviceContext);
	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_TEST_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_TEST_DEPTH_LESS_EQUAL);
}

void RenderPlatform::EndFrame(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::EndFrame(deviceContext);
}

namespace
{
    const float DEFAULT_LIGHT_POSITION[]				={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]		={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]			={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]					={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF				=180.0f;
}

void RenderPlatform::CopyTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *t,crossplatform::Texture *s)
{
	if(!t||!s)
	{
		SIMUL_BREAK_ONCE("Can't copy null texture");
		return;
	}
	ID3D11Resource *T=t->AsD3D11Resource();
	ID3D11Resource *S=s->AsD3D11Resource();
	if(T!=nullptr&&S!=nullptr)
	{
		deviceContext.asD3D11DeviceContext()->CopyResource(T,S);
	}
	else
	{
		return;
	}
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
	ApplyContextState(deviceContext);
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
#ifdef _XBOX_ONE
	if(UsesFastSemantics())
	{
		// Does this draw call use any fenced resources?
		WaitForFencedResources(deviceContext);
	}
#endif
	deviceContext.asD3D11DeviceContext()->Dispatch(w,l,d);
#ifdef _XBOX_ONE
	if(UsesFastSemantics())
	{
		// assign this fence to all active UAV textures.
		crossplatform::ContextState *cs=GetContextState(deviceContext);
		if(cs->currentTechnique&&cs->currentTechnique->shouldFenceOutputs())
		{
			fence=((ID3D11DeviceContextX*)pContext)->InsertFence(0);
			for(auto i=cs->rwTextureAssignmentMap.begin();i!=cs->rwTextureAssignmentMap.end();++i)
			{
				int slot=i.first;
				const crossplatform::TextureAssignment &ta=cs->rwTextureAssignmentMap[slot];
				if(ta.texture&&!ta.texture->IsUnfenceable())
					ta.texture->SetFence(fence);
			}
		}
	}
#endif
}

void RenderPlatform::Draw			(crossplatform::GraphicsDeviceContext &deviceContext,int num_verts,int start_vert)
{
	ApplyContextState(deviceContext);
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->Draw(num_verts,start_vert);
}

void RenderPlatform::DrawIndexed(crossplatform::GraphicsDeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	ApplyContextState(deviceContext);
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->DrawIndexed(num_indices,start_index,base_vert);
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const float BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
/*    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);*/
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
	tex=new dx11::Texture();
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
	return new dx11::Framebuffer(name);
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
	dx11::SamplerState *s=new dx11::SamplerState(d);
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = toD3d11Filter (d->filtering) ;
    samplerDesc.AddressU = toD3d11TextureAddressMode(d->x);
    samplerDesc.AddressV = toD3d11TextureAddressMode(d->y);
    samplerDesc.AddressW = toD3d11TextureAddressMode(d->z);
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	SAFE_RELEASE(s->m_pd3D11SamplerState);
	AsD3D11Device()->CreateSamplerState(&samplerDesc,&s->m_pd3D11SamplerState);
	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect()
{
	crossplatform::Effect *e= new dx11::Effect();
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx11::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx11::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx11::Buffer();
	return b;
}
crossplatform::Shader *RenderPlatform::CreateShader()
{
	return new Shader;
}

crossplatform::DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
    return new dx11::DisplaySurface();
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p,crossplatform::CompressionFormat c)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGB_16_FLOAT:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC6H:
			return DXGI_FORMAT_BC6H_UF16;
		case crossplatform::CompressionFormat::BC7_M6_OPAQUE_ONLY:
			return DXGI_FORMAT_BC7_UNORM;
		default:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		};
	case RGB_11_11_10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC7_M6_OPAQUE_ONLY:
			return DXGI_FORMAT_BC7_UNORM;
		default:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		};
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
		switch (c)
		{
		case crossplatform::CompressionFormat::BC1:
			return DXGI_FORMAT_BC1_UNORM;
		case crossplatform::CompressionFormat::BC3:
			return DXGI_FORMAT_BC3_UNORM;
		default:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		};
	case BGRA_8_UNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC1:
			return DXGI_FORMAT_BC1_UNORM;
		case crossplatform::CompressionFormat::BC3:
			return DXGI_FORMAT_BC3_UNORM;
		default:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		};
	case RGBA_8_UNORM_SRGB:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC1:
			return DXGI_FORMAT_BC1_UNORM_SRGB;
		case crossplatform::CompressionFormat::BC3:
			return DXGI_FORMAT_BC3_UNORM_SRGB;
		default:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		};
	case RGBA_8_UNORM_COMPRESSED:
		return DXGI_FORMAT_BC7_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC4:
			return DXGI_FORMAT_BC4_UNORM;
		default:
			return DXGI_FORMAT_R8_UNORM;
		};
	case R_8_SNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC4:
			return DXGI_FORMAT_BC4_SNORM;
		default:
			return DXGI_FORMAT_R8_SNORM;
		};
	case RG_8_SNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC4:
			return DXGI_FORMAT_BC5_SNORM;
		default:
			return DXGI_FORMAT_R8G8_SNORM;
		};
	case RG_8_UNORM:
		switch (c)
		{
		case crossplatform::CompressionFormat::BC4:
			return DXGI_FORMAT_BC5_UNORM;
		default:
			return DXGI_FORMAT_R8G8_UNORM;
		};
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
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return RGBA_8_UNORM_SRGB;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return BGRA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
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

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc,bool interleaved)
{
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
	dx11::Layout *l=new dx11::Layout();
	l->SetDesc(desc,num_elements, interleaved);
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
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			format = "half4";
			break;
		case DXGI_FORMAT_R16G16_FLOAT:
			format = "half2";
			break;
		case DXGI_FORMAT_R16_FLOAT:
			format = "half";
			break;
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
	V_CHECK(AsD3D11Device()->CreateInputLayout(decl, num_elements, VS->GetBufferPointer(), VS->GetBufferSize(), &l->d3d11InputLayout));
	
	if(VS)
		VS->Release();
	if(errorMsgs)
		errorMsgs->Release();
	delete [] decl;
	return l;
}
static D3D11_COMPARISON_FUNC toD3dComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_ALWAYS:
		return D3D11_COMPARISON_ALWAYS;
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


D3D11_FILL_MODE toD3d11FillMode(crossplatform::PolygonMode p)
{
	switch (p)
	{
	case crossplatform::POLYGON_MODE_FILL:
		return D3D11_FILL_SOLID;
	case crossplatform::POLYGON_MODE_LINE:
		return D3D11_FILL_WIREFRAME;
	case crossplatform::POLYGON_MODE_POINT:
		SIMUL_BREAK("DirectX11 doesn't have a POINT polygon mode");
		return D3D11_FILL_SOLID;
	default:
		SIMUL_BREAK("Undefined polygon mode");
		return D3D11_FILL_SOLID;
	}
}

D3D11_CULL_MODE toD3d11CullMode(crossplatform::CullFaceMode c)
{
	switch (c)
	{
	case simul::crossplatform::CULL_FACE_NONE:
		return D3D11_CULL_NONE;
	case simul::crossplatform::CULL_FACE_FRONT:
		return D3D11_CULL_FRONT;
	case simul::crossplatform::CULL_FACE_BACK:
		return D3D11_CULL_BACK;
	case simul::crossplatform::CULL_FACE_FRONTANDBACK:
		SIMUL_BREAK("In DirectX11 there is no FRONTANDBACK cull face mode");
	default:
		SIMUL_BREAK("Undefined cull face mode");
		return D3D11_CULL_NONE;
	}
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx11::RenderState *s=new dx11::RenderState();
	s->type=desc.type;
	if(desc.type==crossplatform::BLEND)
	{
		D3D11_BLEND_DESC omDesc;
		ZeroMemory( &omDesc, sizeof( D3D11_BLEND_DESC ) );
		for(int i=0;i<desc.blend.numRTs;i++)
		{
			omDesc.RenderTarget[i].BlendEnable				=(desc.blend.RenderTarget[i].blendOperation!=crossplatform::BLEND_OP_NONE)||(desc.blend.RenderTarget[i].blendOperationAlpha!=crossplatform::BLEND_OP_NONE);
			omDesc.RenderTarget[i].BlendOp					=toD3dBlendOp(desc.blend.RenderTarget[i].blendOperation);
			omDesc.RenderTarget[i].SrcBlend					=toD3dBlend(desc.blend.RenderTarget[i].SrcBlend);
			omDesc.RenderTarget[i].DestBlend				=toD3dBlend(desc.blend.RenderTarget[i].DestBlend);
			omDesc.RenderTarget[i].BlendOpAlpha				=toD3dBlendOp(desc.blend.RenderTarget[i].blendOperationAlpha);
			omDesc.RenderTarget[i].SrcBlendAlpha			=toD3dBlend(desc.blend.RenderTarget[i].SrcBlendAlpha);
			omDesc.RenderTarget[i].DestBlendAlpha			=toD3dBlend(desc.blend.RenderTarget[i].DestBlendAlpha);
			omDesc.RenderTarget[i].RenderTargetWriteMask	=desc.blend.RenderTarget[i].RenderTargetWriteMask;
		}
		omDesc.IndependentBlendEnable			=desc.blend.IndependentBlendEnable;
		omDesc.AlphaToCoverageEnable			=desc.blend.AlphaToCoverageEnable;
		AsD3D11Device()->CreateBlendState(&omDesc,&s->m_blendState);
		SetDebugObjectName(s->m_blendState, "Blend State");
	}
	if (desc.type == crossplatform::RASTERIZER)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc;
		ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));

		rasterizerDesc.FillMode = toD3d11FillMode(desc.rasterizer.polygonMode);
		rasterizerDesc.CullMode = toD3d11CullMode(desc.rasterizer.cullFaceMode);
		rasterizerDesc.FrontCounterClockwise = (desc.rasterizer.frontFace != crossplatform::FrontFace::FRONTFACE_CLOCKWISE);
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.SlopeScaledDepthBias = 0.0f;
		rasterizerDesc.DepthClipEnable = false;
		rasterizerDesc.MultisampleEnable = false;	// TO-DO : what if not!!!
		rasterizerDesc.AntialiasedLineEnable = false;
		AsD3D11Device()->CreateRasterizerState(&rasterizerDesc, &s->m_rasterizerState);
	}
	else if(desc.type==crossplatform::DEPTH)
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		//Initialize the description of the stencil state.
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		// Set up the description of the stencil state.
		depthStencilDesc.DepthEnable		=desc.depth.test;
		depthStencilDesc.DepthWriteMask		=desc.depth.write? D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc			= toD3dComparison(desc.depth.comparison);

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Stencil operations if pixel is front-facing.
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Stencil operations if pixel is back-facing.
		depthStencilDesc.BackFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp= D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc		= D3D11_COMPARISON_ALWAYS;
		
		AsD3D11Device()->CreateDepthStencilState(&depthStencilDesc, &s->m_depthStencilState);
	}
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	dx11::Query *q=new dx11::Query(type);
	return q;
}

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers
	, const crossplatform::Buffer *const*buffers
	,const crossplatform::Layout *layout
	,const int *vertexSteps)
{
	ID3D11Buffer * buf[10] = {};
	UINT strides[10];
	UINT offsets[10];
	for(int i=0;i<num_buffers;i++)
	{
		if (buffers)
		{
			strides[i]=buffers[i]->stride;
			if(vertexSteps&&vertexSteps[i]>=1)
				strides[i]*=vertexSteps[i];
			buf[i]=buffers[i]->AsD3D11Buffer();
		}
		else
		{
			buf[i] = nullptr;
			strides[i] = 0;
		}

		offsets[i]=0;
	}
	ID3D11Buffer *const *b2 = buf;
	if(num_buffers)
		deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(	0,	// the first input slot for binding
									num_buffers,					// the number of buffers in the array
									buf,							// the array of vertex buffers
									strides,						// array of stride values, one for each buffer
									offsets );						// array of offset values, one for each buffer
	else
		deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(	0,	// the first input slot for binding
									0,								// the number of buffers in the array
									nullptr,						// the array of vertex buffers
									strides,						// array of stride values, one for each buffer
									offsets );						// array of offset values, one for each buffer
};

void RenderPlatform::ClearVertexBuffers(crossplatform::DeviceContext& deviceContext)
{

}

void RenderPlatform::SetStreamOutTarget(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Buffer *vertexBuffer,int start_index)
{
	ID3D11Buffer *b=NULL;
	if(vertexBuffer)
		b=vertexBuffer->AsD3D11Buffer();
	UINT offset = vertexBuffer?(vertexBuffer->stride*start_index):0;
	deviceContext.asD3D11DeviceContext()->SOSetTargets(1,&b,&offset );
}

void RenderPlatform::ApplyDefaultRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(!deviceContext.asD3D11DeviceContext())
		return;
	auto context=deviceContext.asD3D11DeviceContext();
	D3D11_VIEWPORT viewport;
	ID3D11RenderTargetView **rtv=(ID3D11RenderTargetView **)deviceContext.defaultTargetsAndViewport.m_rt;
	ID3D11DepthStencilView *dsv=(ID3D11DepthStencilView*)deviceContext.defaultTargetsAndViewport.m_dt;
	context->OMSetRenderTargets(1,rtv,dsv);
	
	viewport.Width		=(float)(deviceContext.defaultTargetsAndViewport.viewport.w);
	viewport.Height		=(float)(deviceContext.defaultTargetsAndViewport.viewport.h);
	viewport.TopLeftX	=(float)deviceContext.defaultTargetsAndViewport.viewport.x;
	viewport.TopLeftY	=(float)deviceContext.defaultTargetsAndViewport.viewport.y;
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	context->RSSetViewports(1, &viewport);
}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	ID3D11RenderTargetView *rt[8];
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
		rt[i]=targs[i]->AsD3D11RenderTargetView();
	ID3D11DepthStencilView *d=NULL;
	if(depth)
		d=depth->AsD3D11DepthStencilView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(num,rt,d);

	int w=targs[0]->width, h = targs[0]->length;
	crossplatform::Viewport v[] = { { 0, 0, w, h },{ 0, 0, w, h },{ 0, 0, w, h },
									{ 0, 0, w, h },{ 0, 0, w, h },{ 0, 0, w, h },
									{ 0, 0, w, h },{ 0, 0, w, h }};

	crossplatform::TargetsAndViewport *targetAndViewport=new crossplatform::TargetsAndViewport;
	targetAndViewport->temp=true;
	targetAndViewport->num=num;
	for(int i=0;i<num;i++)
	{
		if(targs[i]&&targs[i]->IsValid())
		{
			targetAndViewport->m_rt[i]=rt[i];
		}
	}
	targetAndViewport->m_dt=d;
	targetAndViewport->viewport.x=targetAndViewport->viewport.y=0;
	targetAndViewport->viewport.w=targs[0]->width;
	targetAndViewport->viewport.h=targs[0]->length;
	PushRenderTargets(deviceContext,targetAndViewport);

	SetViewports(deviceContext,num,v);
}

void RenderPlatform::DeactivateRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext)
{
	PopRenderTargets(deviceContext);
}

void RenderPlatform::SetViewports(crossplatform::GraphicsDeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	if(!vps)
		return;
	D3D11_VIEWPORT viewports[8];
	SIMUL_ASSERT(num<=8);
	for(int i=0;i<num;i++)
	{
		viewports[i].Width		=(float)vps[i].w;
		viewports[i].Height		=(float)vps[i].h;
		viewports[i].TopLeftX	=(float)vps[i].x;
		viewports[i].TopLeftY	=(float)vps[i].y;
		viewports[i].MinDepth	=0.0f;
		viewports[i].MaxDepth	=1.0f;
	}
	deviceContext.asD3D11DeviceContext()->RSSetViewports(num,viewports);
	crossplatform::RenderPlatform::SetViewports(deviceContext,num,vps);
}

void RenderPlatform::SetIndexBuffer(crossplatform::GraphicsDeviceContext &deviceContext, const crossplatform::Buffer *buffer)
{
	if(!buffer)
	{
		deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
		return;
	}
	DXGI_FORMAT f;
	int index_byte_size=buffer->stride;
	switch(index_byte_size)
	{
	case 4:
		f=DXGI_FORMAT_R32_UINT;
		break;
	case 2:
		f=DXGI_FORMAT_R16_UINT;
		break;
	default:
		f=DXGI_FORMAT_UNKNOWN;
		SIMUL_BREAK("Can't use DXGI_FORMAT_UNKNOWN for an index buffer.")
		break;
	};
	deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(buffer->AsD3D11Buffer(), f, 0);
}

static D3D11_PRIMITIVE_TOPOLOGY toD3dTopology(crossplatform::Topology t)
{
	using namespace crossplatform;
	switch(t)
	{			
	case Topology::POINTLIST:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case Topology::LINELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case Topology::LINESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case Topology::TRIANGLELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case Topology::TRIANGLESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case Topology::LINELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case Topology::LINESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case Topology::TRIANGLELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case Topology::TRIANGLESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	default:
		return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};
}

void RenderPlatform::SetTopology(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Topology t)
{
	D3D11_PRIMITIVE_TOPOLOGY T=toD3dTopology(t);
	deviceContext.asD3D11DeviceContext()->IASetPrimitiveTopology(T);
}

void RenderPlatform::SetLayout(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Layout *l)
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
	if(!s)
		return;
	dx11::RenderState *ds=(dx11::RenderState *)(s);
	if(ds->type==crossplatform::BLEND)
	{
		float blendFactor[]		={0,0,0,0};
		UINT sampleMask			=0xffffffff;
		deviceContext.asD3D11DeviceContext()->OMSetBlendState(ds->m_blendState,blendFactor,sampleMask);
	}
	if(ds->type==crossplatform::DEPTH)
	{
		deviceContext.asD3D11DeviceContext()->OMSetDepthStencilState(ds->m_depthStencilState,0);
	}
}

void RenderPlatform::Resolve(crossplatform::GraphicsDeviceContext &deviceContext,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	deviceContext.asD3D11DeviceContext()->ResolveSubresource(destination->AsD3D11Texture2D(),0,source->AsD3D11Texture2D(),0,dx11::RenderPlatform::ToDxgiFormat(destination->GetFormat()));
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
	dx11::SaveTexture(device,texture->AsD3D11Texture2D(),lFileNameUtf8);
}

void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	if(!can_save_and_restore)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(storedStateCursor>=storedStates.size())
	{
		storedStates.resize(std::max(1,storedStateCursor*2));
	}
	StoredState &s=storedStates[storedStateCursor++];
	pContext->OMGetDepthStencilState( &s.m_pDepthStencilStateStored11, &s.m_StencilRefStored11 );
	pContext->RSGetState(&s.m_pRasterizerStateStored11 );
	pContext->OMGetBlendState(&s.m_pBlendStateStored11,s.m_BlendFactorStored11, &s.m_SampleMaskStored11 );
	pContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pSamplerStateStored11 );
	pContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pVertexSamplerStateStored11 );
	
    pContext->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pGeometrySamplerStateStored11 );
    pContext->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pComputeSamplerStateStored11 );
	
	pContext->VSGetShader(&s.pVertexShader,s.m_pVertexClassInstances,&s.numVertexClassInstances);
	pContext->PSGetShader(&s.pPixelShader,s.m_pPixelClassInstances,&s.numPixelClassInstances);
	pContext->HSGetShader(&s.pHullShader,s.m_pHullClassInstances,&s.numHullClassInstances);
	pContext->DSGetShader(&s.pDomainShader,s.m_pDomainClassInstances,&s.numDomainClassInstances);
	pContext->GSGetShader(&s.pGeometryShader,s.m_pGeometryClassInstances,&s.numGeometryClassInstances);
	pContext->CSGetShader(&s.pComputeShader,s.m_pPixelClassInstances,&s.numPixelClassInstances);

	pContext->CSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pCSConstantBuffers);
	pContext->GSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pGSConstantBuffers);
	pContext->PSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pPSConstantBuffers);
	pContext->VSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pVSConstantBuffers);
	pContext->HSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pHSConstantBuffers);
	pContext->DSGetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pDSConstantBuffers);
	
	pContext->PSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pPSShaderResourceViews);
	pContext->CSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pCSShaderResourceViews);
	pContext->VSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pVSShaderResourceViews);
	pContext->HSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pHSShaderResourceViews);
	pContext->GSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pGSShaderResourceViews);
	pContext->DSGetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pDSShaderResourceViews);





	pContext->CSGetUnorderedAccessViews(0,D3D11_PS_CS_UAV_REGISTER_COUNT,s.m_pUnorderedAccessViews);

	pContext->IAGetInputLayout( &s.m_previousInputLayout );
	pContext->IAGetPrimitiveTopology(&s.m_previousTopology);

	pContext->IAGetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);

	pContext->IAGetIndexBuffer(&s.pIndexBufferStored11,
								&s.m_indexFormatStored11,
								&s.m_indexOffset);
//	pContext->RSGetScissorRects(&s.m_numRects,s.m_scissorRects);
}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	if (!can_save_and_restore)
		return;
	storedStateCursor--;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	StoredState &s=storedStates[storedStateCursor];

    pContext->OMSetDepthStencilState(s.m_pDepthStencilStateStored11,s.m_StencilRefStored11 );
    SAFE_RELEASE_SILENT(s.m_pDepthStencilStateStored11);

	pContext->RSSetState(s.m_pRasterizerStateStored11);
    SAFE_RELEASE_SILENT(s.m_pRasterizerStateStored11);

    pContext->OMSetBlendState(s.m_pBlendStateStored11,s.m_BlendFactorStored11,s.m_SampleMaskStored11 );
    SAFE_RELEASE_SILENT(s.m_pBlendStateStored11);
	
    pContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pSamplerStateStored11 );
    pContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pVertexSamplerStateStored11 );
    pContext->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pGeometrySamplerStateStored11 );
    pContext->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pComputeSamplerStateStored11 );
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE_SILENT(s.m_pSamplerStateStored11[i]);
	    SAFE_RELEASE_SILENT(s.m_pVertexSamplerStateStored11[i]);
	    SAFE_RELEASE_SILENT(s.m_pComputeSamplerStateStored11[i]);
	    SAFE_RELEASE_SILENT(s.m_pGeometrySamplerStateStored11[i]);
	}
	
	pContext->VSSetShader(s.pVertexShader,s.m_pVertexClassInstances,s.numVertexClassInstances);
    SAFE_RELEASE_SILENT(s.pVertexShader );
	pContext->PSSetShader(s.pPixelShader,s.m_pPixelClassInstances,s.numPixelClassInstances);
    SAFE_RELEASE_SILENT(s.pPixelShader );
	pContext->HSSetShader(s.pHullShader,s.m_pHullClassInstances,s.numHullClassInstances);
	SAFE_RELEASE_SILENT(s.pHullShader);
	pContext->DSSetShader(s.pDomainShader,s.m_pDomainClassInstances,s.numDomainClassInstances);
	SAFE_RELEASE_SILENT(s.pDomainShader);
	pContext->GSSetShader(s.pGeometryShader,s.m_pGeometryClassInstances,s.numGeometryClassInstances);
	SAFE_RELEASE_SILENT(s.pGeometryShader);
	pContext->CSSetShader(s.pComputeShader,s.m_pPixelClassInstances,s.numPixelClassInstances);
	SAFE_RELEASE_SILENT(s.pComputeShader);
	pContext->CSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pCSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pCSConstantBuffers[i]);
	}
	pContext->GSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pGSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pGSConstantBuffers[i]);
	}
	pContext->PSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pPSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pPSConstantBuffers[i]);
	}
	pContext->VSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pVSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pVSConstantBuffers[i]);
	}
	pContext->HSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pHSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pHSConstantBuffers[i]);
	}
	pContext->DSSetConstantBuffers(0,D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT,s.m_pDSConstantBuffers);
	for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pDSConstantBuffers[i]);
	}
	pContext->PSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pPSShaderResourceViews);
	pContext->CSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pCSShaderResourceViews);
	pContext->VSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pVSShaderResourceViews);
	pContext->HSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pHSShaderResourceViews);
	pContext->GSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pGSShaderResourceViews);
	pContext->DSSetShaderResources(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,s.m_pDSShaderResourceViews);



	for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pPSShaderResourceViews[i]);
		SAFE_RELEASE_SILENT(s.m_pCSShaderResourceViews[i]);
		SAFE_RELEASE_SILENT(s.m_pVSShaderResourceViews[i]);
		SAFE_RELEASE_SILENT(s.m_pHSShaderResourceViews[i]);
		SAFE_RELEASE_SILENT(s.m_pGSShaderResourceViews[i]);
		SAFE_RELEASE_SILENT(s.m_pDSShaderResourceViews[i]);
	}
	// TODO: handle produce-consume buffers below
	pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, s.m_pUnorderedAccessViews, NULL);
	for (int i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
	{
		SAFE_RELEASE_SILENT(s.m_pUnorderedAccessViews[i]);
	}

	pContext->IASetPrimitiveTopology(s.m_previousTopology);
	pContext->IASetInputLayout(s.m_previousInputLayout );
	SAFE_RELEASE_SILENT(s.m_previousInputLayout);

	pContext->IASetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE_SILENT(s.m_pVertexBuffersStored11[i]);
	}

	pContext->IASetIndexBuffer(s.pIndexBufferStored11,
								s.m_indexFormatStored11,
								s.m_indexOffset);
	SAFE_RELEASE_SILENT(s.pIndexBufferStored11);

	//pContext->RSSetScissorRects(s.m_numRects,s.m_scissorRects);
#ifndef _XBOX_ONE
#endif
}

crossplatform::RenderPlatformType RenderPlatform::GetType() const
{
	return crossplatform::RenderPlatformType::D3D11;
}

bool RenderPlatform::ApplyContextState(crossplatform::DeviceContext &deviceContext, bool /*error_checking*/) 
{
	if (deviceContext.contextState.currentLayout)
	{
		auto *l = static_cast<const dx11::Layout *>(deviceContext.contextState.currentLayout);
		deviceContext.asD3D11DeviceContext()->IASetInputLayout(l->AsD3D11InputLayout());
	}
#if PLATFORM_D3D11_SFX
	crossplatform::ContextState* cs = GetContextState(deviceContext);
	dx11::EffectPass* pass = static_cast<dx11::EffectPass*>(cs->currentEffectPass);
	if (!cs->effectPassValid)
	{
		// This applies the pass, and also any associated state: Blend, Depth and Rasterizer states:
		pass->Apply(deviceContext);
		cs->effectPassValid = true;
	}
	pass->SetSamplers(deviceContext, cs->currentEffect->GetSamplers() );
	
	pass->SetConstantBuffers(deviceContext,cs->applyBuffers);
	// Apply SRVs (textures and SB):
	pass->SetSRVs(deviceContext,cs->textureAssignmentMap, cs->applyStructuredBuffers);

	// Apply UAVs (RwTextures and RwSB):
	pass->SetUAVs(deviceContext,cs->rwTextureAssignmentMap, cs->applyRwStructuredBuffers);
#endif
	return true;
}

void RenderPlatform::WaitForFencedResources(crossplatform::DeviceContext &deviceContext)
{
#ifdef _XBOX_ONE
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	crossplatform::ContextState *contextState=GetContextState(deviceContext);
	for(auto i=contextState->textureAssignmentMap.begin();i!=contextState->textureAssignmentMap.end();++i)
	{
		int slot=i.first;
		const crossplatform::TextureAssignment &ta=contextState->textureAssignmentMap[i.first];
		if(!ta.texture)
			continue;
		// don't need to wait for a writeable texture. PROBABLY
		if(slot<0||slot>=1000)
			continue;
		unsigned long long fence=ta.texture->GetFence();
		if(fence)
		{
			((ID3D11DeviceContextX*)pContext)->InsertWaitOnFence(0,fence);
			ta.texture->ClearFence();
		}
	}
#endif
}

void RenderPlatform::DrawQuad(crossplatform::GraphicsDeviceContext &deviceContext)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	crossplatform::RenderPlatform::SetTopology(deviceContext,simul::crossplatform::Topology::TRIANGLESTRIP);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(NULL);
	ApplyContextState(deviceContext);
#ifdef _XBOX_ONE
	if(UsesFastSemantics())
	{
		// Does this draw call use any fenced resources?
		WaitForFencedResources(deviceContext);
	}
#endif
	pContext->Draw(4,0);
#ifdef _XBOX_ONE
	// Fence the rendertarget:
	if(UsesFastSemantics())
	{
		fence=((ID3D11DeviceContextX*)pContext)->InsertFence(0);
		// assign this fence to all active texture targets.
		crossplatform::ContextState *cs=GetContextState(deviceContext);
		for(auto i=cs->rwTextureAssignmentMap.begin();i!=cs->rwTextureAssignmentMap.end();++i)
		{
			//int slot=i.first;
			const crossplatform::TextureAssignment &ta=cs->rwTextureAssignmentMap[i.first];;
			if(ta.texture&&!ta.texture->IsUnfenceable())
				ta.texture->SetFence(fence);
		}
	}
#endif
}

void RenderPlatform::PopRenderTargets(crossplatform::GraphicsDeviceContext &deviceContext)
{
	std::stack<crossplatform::TargetsAndViewport*> &fbs=deviceContext.GetFrameBufferStack();
	crossplatform::TargetsAndViewport *oldtv=fbs.top();
	fbs.pop();
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();

	crossplatform::TargetsAndViewport *state=&deviceContext.defaultTargetsAndViewport;
	if(fbs.size())
		state=fbs.top();
	ID3D11RenderTargetView **rt=(ID3D11RenderTargetView**)state->m_rt;
	ID3D11DepthStencilView *dt=(ID3D11DepthStencilView*)state->m_dt;
	pContext->OMSetRenderTargets(	state->num,
									rt,
									dt);
	D3D11_VIEWPORT viewports[8];
	for(int i=0;i<state->num;i++)
	{
		viewports[i].Width		=(float)state->viewport.w;
		viewports[i].Height		=(float)state->viewport.h;
		viewports[i].TopLeftX	=(float)state->viewport.x;
		viewports[i].TopLeftY	=(float)state->viewport.y;
		viewports[i].MinDepth	=0.0f;
		viewports[i].MaxDepth	=1.0f;
	}
	pContext->RSSetViewports(state->num,viewports);
	//unsigned num=1;
	//pContext->RSGetViewports(&num,viewports);
	for(int i=0;i<(int)oldtv->num;i++)
	{
		ID3D11RenderTargetView *r=(ID3D11RenderTargetView*)oldtv->m_rt[i];
	}
	ID3D11DepthStencilView *d=(ID3D11DepthStencilView*)oldtv->m_dt;
	//SetViewports(deviceContext,state->num,&state->viewport);
	if(oldtv->temp)
		delete oldtv;
}
