// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulHDRRendererDX1x.cpp A renderer for skies, clouds and weather effects.
#define NOMINMAX
#include "SimulHDRRendererDX1x.h"


#include <tchar.h>
#include <dxerr.h>
#include <string>
#include <assert.h>
typedef std::basic_string<TCHAR> tstring;

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "MacrosDX1x.h"

#define BLUR_SIZE 9

#include "Simul/Math/Pi.h"

SimulHDRRendererDX1x::SimulHDRRendererDX1x(int w,int h)
	:m_pd3dDevice(NULL)
	,m_pImmediateContext(NULL)
	,m_pVertexBuffer(NULL)
	,m_pTonemapEffect(NULL)
	,m_pGaussianEffect(NULL)
	,Exposure(1.f)
	,Gamma(1.f/2.2f)			// no need for shader-based gamma-correction with DX10/11
	,Width(w)
	,Height(h)
	,timing(0.f)
	,framebuffer(w,h)
	,glow_fb(w/2,h/2)
	,TonemapTechnique(NULL)
	,glowTechnique(NULL)
	,Exposure_(NULL)
	,Gamma_(NULL)
	,imageTexture(NULL)
	,worldViewProj(NULL)
{
}

void SimulHDRRendererDX1x::SetBufferSize(int w,int h)
{
	Width=w;
	Height=h;
	framebuffer.SetWidthAndHeight(w,h);
	InvalidateDeviceObjects();
}

void SimulHDRRendererDX1x::RestoreDeviceObjects(void *x)
{
	HRESULT hr=S_OK;
	void **u=(void**)x;
	m_pd3dDevice=(ID3D1xDevice*)u[0];
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	glow_fb.RestoreDeviceObjects(m_pd3dDevice);
	ID3D1xTexture2D *pBackBuffer=NULL;
	IDXGISwapChain *pSwapChain=(IDXGISwapChain *)u[1];
	pSwapChain->GetBuffer(0,__uuidof(ID3D1xTexture2D),(void**)&pBackBuffer);
	D3D1x_TEXTURE2D_DESC desc;
	pBackBuffer->GetDesc(&desc);
	SAFE_RELEASE(pBackBuffer);

	framebuffer.SetTargetWidthAndHeight(desc.Width,desc.Height);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	glowTexture.release();
	glowTexture.init(m_pd3dDevice,Width/2,Height/2);
	glow_fb.SetWidthAndHeight(Width/2,Height/2);
	RecompileShaders();
}

static std::string string_format(const std::string fmt, ...)
{
    int size = 100;
    std::string str;
    va_list ap;
    while (1)
	{
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size)
		{
            str.resize(n);
            return str;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    return str;
}

template<typename t> t max3(t a,t b,t c)
{
	return std::max(a,std::max(b,c));
}

void SimulHDRRendererDX1x::RecompileShaders()
{
	SAFE_RELEASE(m_pTonemapEffect);
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,_T("simul_hdr.fx"));
	TonemapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_tonemap");
	glowTechnique			=m_pTonemapEffect->GetTechniqueByName("simul_glow");
	Exposure_				=m_pTonemapEffect->GetVariableByName("exposure")->AsScalar();
	Gamma_					=m_pTonemapEffect->GetVariableByName("gamma")->AsScalar();
	imageTexture			=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	worldViewProj			=m_pTonemapEffect->GetVariableByName("worldViewProj")->AsMatrix();
		
	SAFE_RELEASE(m_pGaussianEffect);
	
	int	threadsPerGroup = 256;

//static const uint THREADS_PER_GROUP = 128;
//static const uint SCAN_SMEM_SIZE = 1200;
//static const uint TEXELS_PER_THREAD = 5;
//static const uint NUM_IMAGE_ROWS = 1200;
//static const uint NUM_IMAGE_COLS = 1600;
	int W=Width;
	int H=Height;
	std::map<std::string,std::string> defs;
	int scan_smem_size			=max3(H,W,(int)threadsPerGroup*2);
	int texels_per_thread = (H + threadsPerGroup - 1) / threadsPerGroup;
//	int texels_per_thread		= (std::max(H,W) + threadsPerGroup - 1) / threadsPerGroup;
	defs["SCAN_SMEM_SIZE"]	=string_format("%d",scan_smem_size);
	defs["TEXELS_PER_THREAD"]=string_format("%d",texels_per_thread);
	defs["THREADS_PER_GROUP"]=string_format("%d",threadsPerGroup);
	defs["NUM_IMAGE_COLS"]	=string_format("%d",W);
	defs["NUM_IMAGE_ROWS"]	=string_format("%d",H);
	
//	CreateEffect(m_pd3dDevice,&m_pGaussianEffect,_T("simul_gaussian.fx"),defs);
}

void SimulHDRRendererDX1x::InvalidateDeviceObjects()
{
	framebuffer.InvalidateDeviceObjects();
	glow_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pGaussianEffect);
	glowTexture.release();
}

bool SimulHDRRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return true;
}

SimulHDRRendererDX1x::~SimulHDRRendererDX1x()
{
	Destroy();
}

bool SimulHDRRendererDX1x::StartRender()
{
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::StartRender");
	if(imageTexture)
		imageTexture->SetResource(NULL);
	framebuffer.Activate();
	framebuffer.Clear(0,0,0,0);

	PIXEndNamedEvent();
	return true;
}

bool SimulHDRRendererDX1x::ApplyFade()
{
	HRESULT hr=S_OK;
	return (hr==S_OK);
}

bool SimulHDRRendererDX1x::FinishRender()
{
	PIXBeginNamedEvent(0,"SimulHDRRendererDX1x::FinishRender");
	framebuffer.Deactivate();
	imageTexture->SetResource(framebuffer.GetBufferResource());//buffer_texture_SRV);
	Gamma_->SetFloat(Gamma);
	Exposure_->SetFloat(Exposure);
	D3DXMATRIX ortho;
	D3DXMatrixIdentity(&ortho);
    D3DXMatrixOrthoLH(&ortho,2.f,2.f,-100.f,100.f);
	worldViewProj->SetMatrix(ortho);
RenderGlowTexture();
simul::dx11::setParameter(m_pTonemapEffect,"glowTexture",glowTexture.g_pSRV_Output);
	ApplyPass(TonemapTechnique->GetPassByIndex(0));
	framebuffer.DrawQuad();
	imageTexture->SetResource(NULL);
	ApplyPass(TonemapTechnique->GetPassByIndex(0));
	PIXEndNamedEvent();
	return true;
}

static float CalculateBoxFilterWidth(float radius, int pass)
{
	// Calculate standard deviation according to cutoff width
	// We use sigma*3 as the width of filtering window
	float sigma = radius / 3.0f;
	// The width of the repeating box filter
	float box_width = sqrt(sigma * sigma * 12.0f / pass + 1);
	return box_width;
}

void SimulHDRRendererDX1x::RenderGlowTexture()
{
	if(!m_pGaussianEffect)
		return;
static int g_NumApproxPasses=3;
static int	g_MaxApproxPasses = 8;
static float g_FilterRadius = 30;
	// Render to the low-res glow.
	if(glowTechnique)
	{
		imageTexture->SetResource(framebuffer.GetBufferResource());
		simul::dx11::setParameter(m_pTonemapEffect,"offset",1.f/Width,1.f/Height);
		ApplyPass(glowTechnique->GetPassByIndex(0));
		glow_fb.Activate();
		glow_fb.DrawQuad();
		glow_fb.Deactivate();
	}
    D3D11_TEXTURE2D_DESC tex_desc;
	ID3D1xTexture2D *texture=glow_fb.GetColorTexResource();
	texture->GetDesc(&tex_desc);

	float box_width = CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width = box_width * 0.5f;
	float frac_half_box_width = (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width = 1.0f - frac_half_box_width;
	float rcp_box_width = 1.0f / box_width;
	// Step 1. Vertical passes: Each thread group handles a colomn in the image
	// Input texture
	simul::dx11::setParameter(m_pGaussianEffect,"g_texInput",(ID3D11ShaderResourceView*)glow_fb.GetColorTex());
	// Output texture
	simul::dx11::setParameter(m_pGaussianEffect,"g_rwtOutput",glowTexture.g_pUAV_Output);
	simul::dx11::setParameter(m_pGaussianEffect,"g_NumApproxPasses",g_NumApproxPasses - 1);
	simul::dx11::setParameter(m_pGaussianEffect,"g_HalfBoxFilterWidth",half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_FracHalfBoxFilterWidth",frac_half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_InvFracHalfBoxFilterWidth",inv_frac_half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_RcpBoxFilterWidth",rcp_box_width);

	// Select pass
	gaussianColTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_col");
	gaussianColTechnique->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	m_pImmediateContext->Dispatch(tex_desc.Width,1,1);

	// Unbound CS resource and output
	ID3D11ShaderResourceView* srv_array[] = {NULL, NULL, NULL, NULL};
	m_pImmediateContext->CSSetShaderResources(0, 4, srv_array);
	ID3D11UnorderedAccessView* uav_array[] = {NULL, NULL, NULL, NULL};
	m_pImmediateContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);

	// Step 2. Horizontal passes: Each thread group handles a row in the image
	// Input texture
	simul::dx11::setParameter(m_pGaussianEffect,"g_texInput",(ID3D11ShaderResourceView*)glow_fb.GetColorTex());
	// Output texture
	simul::dx11::setParameter(m_pGaussianEffect,"g_rwtOutput",glowTexture.g_pUAV_Output);

	// Select pass
	gaussianRowTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_row");
	gaussianRowTechnique->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	m_pImmediateContext->Dispatch(tex_desc.Height,1,1);

	// Unbound CS resource and output
	m_pImmediateContext->CSSetShaderResources(0,4,srv_array);
	m_pImmediateContext->CSSetUnorderedAccessViews(0,4,uav_array, NULL);
}

const char *SimulHDRRendererDX1x::GetDebugText() const
{
	static char debug_text[256];
	return debug_text;
}

float SimulHDRRendererDX1x::GetTiming() const
{
	return timing;
}
