// Copyright (c) 2007-2014 Simul Software Ltd
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
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include <algorithm>			// for std::min / max
#include <assert.h>
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
#include "Utilities.h"
#include "Simul/Math/Pi.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulHDRRendererDX1x::SimulHDRRendererDX1x(int w,int h)
	:m_pd3dDevice(NULL)
	,m_pTonemapEffect(NULL)
	,m_pGaussianEffect(NULL)
	,Glow(false)
	,Width(w)
	,Height(h)
	,timing(0.f)
	,glow_fb(w/2,h/2)
	,exposureGammaTechnique(NULL)
	,glowExposureGammaTechnique(NULL)
	,glowTechnique(NULL)
{
}

void SimulHDRRendererDX1x::SetBufferSize(int w,int h)
{
	if(Width==w&&Height==h)
		return;
	Width=w;
	Height=h;
	if(Width>0&&Height>0)
	{
		if(m_pd3dDevice)
			glowTexture.init(m_pd3dDevice,Width/2,Height/2);
		glow_fb.SetWidthAndHeight(Width/2,Height/2);
	}
	RecompileShaders();
}

void SimulHDRRendererDX1x::RestoreDeviceObjects(void *dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D11Device*)dev;
	glow_fb.RestoreDeviceObjects(m_pd3dDevice);

	glowTexture.release();
	if(m_pd3dDevice&&Width>0&&Height>0)
	{
		glowTexture.init(m_pd3dDevice,Width/2,Height/2);
		glow_fb.SetWidthAndHeight(Width/2,Height/2);
	}
	hdrConstants.RestoreDeviceObjects(m_pd3dDevice);
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
	if(!m_pd3dDevice)
		return;
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,("simul_hdr.fx"));
	exposureGammaTechnique		=m_pTonemapEffect->GetTechniqueByName("exposure_gamma");
	glowExposureGammaTechnique	=m_pTonemapEffect->GetTechniqueByName("glow_exposure_gamma");
	warpExposureGamma			=m_pTonemapEffect->GetTechniqueByName("warp_exposure_gamma");
	warpGlowExposureGamma		=m_pTonemapEffect->GetTechniqueByName("warp_glow_exposure_gamma");
	glowTechnique				=m_pTonemapEffect->GetTechniqueByName("simul_glow");
	hdrConstants.LinkToEffect(m_pTonemapEffect,"HdrConstants");
	SAFE_RELEASE(m_pGaussianEffect);
	
	static int	threadsPerGroup = 128;

//static const uint THREADS_PER_GROUP = 128;
//static const uint SCAN_SMEM_SIZE = 1200;
//static const uint TEXELS_PER_THREAD = 5;
//static const uint NUM_IMAGE_ROWS = 1200;
//static const uint NUM_IMAGE_COLS = 1600;
	int W=Width;
	int H=Height;
	if(!H||!W)
		return;
	std::map<std::string,std::string> defs;
	int scan_smem_size			=max3(H,W,(int)threadsPerGroup*2);
	int texels_per_thread = (H + threadsPerGroup - 1) / threadsPerGroup;
//	int texels_per_thread		= (std::max(H,W) + threadsPerGroup - 1) / threadsPerGroup;
	defs["SCAN_SMEM_SIZE"]	=string_format("%d",scan_smem_size);
	defs["TEXELS_PER_THREAD"]=string_format("%d",texels_per_thread);
	defs["THREADS_PER_GROUP"]=string_format("%d",threadsPerGroup);
	defs["NUM_IMAGE_COLS"]	=string_format("%d",W);
	defs["NUM_IMAGE_ROWS"]	=string_format("%d",H);
	
	CreateEffect(m_pd3dDevice,&m_pGaussianEffect,"simul_gaussian.fx",defs,0);
	hdrConstants.LinkToEffect(m_pGaussianEffect,"HdrConstants");
}

void SimulHDRRendererDX1x::InvalidateDeviceObjects()
{
	hdrConstants.InvalidateDeviceObjects();
	glow_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(m_pTonemapEffect);
	SAFE_RELEASE(m_pGaussianEffect);
	glowTexture.release();
	m_pd3dDevice=NULL;
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

void SimulHDRRendererDX1x::Render(crossplatform::DeviceContext &deviceContext,void *texture_srv,float Exposure,float Gamma)
{
	Render(deviceContext,texture_srv,0,Exposure, Gamma);
}

void SimulHDRRendererDX1x::Render(crossplatform::DeviceContext &deviceContext,void *texture_srv,float offsetX,float Exposure,float Gamma)
{
	ID3D11DeviceContext *pContext		=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"HDR")
	ID3D11ShaderResourceView *textureSRV=(ID3D11ShaderResourceView*)texture_srv;
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	textureSRV->GetDesc(&desc);
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	dx11::setTexture(m_pTonemapEffect,"imageTexture"	,textureSRV);
	dx11::setTexture(m_pTonemapEffect,"imageTextureMS"	,textureSRV);
	hdrConstants.gamma		=Gamma;
	hdrConstants.exposure	=Exposure;
	hdrConstants.Apply(deviceContext);
	simul::dx11::setParameter(m_pTonemapEffect,"offset",offsetX,0.f);
	ID3DX11EffectTechnique *tech=exposureGammaTechnique;
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture_srv);
		tech=glowExposureGammaTechnique;
		simul::dx11::setTexture(m_pTonemapEffect,"glowTexture",glowTexture.g_pSRV_Output);
	}
	ApplyPass(pContext,tech->GetPassByName(msaa?"msaa":"main"));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);

	dx11::setTexture(m_pTonemapEffect,"imageTexture",NULL);
	dx11::setTexture(m_pTonemapEffect,"imageTextureMS",NULL);
	hdrConstants.Unbind(pContext);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	SIMUL_COMBINED_PROFILE_END(pContext)
}

void SimulHDRRendererDX1x::RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,void *texture_srv
	,float offsetX,float Exposure,float Gamma)
{
	ID3D11DeviceContext *pContext		=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView *textureSRV=(ID3D11ShaderResourceView*)texture_srv;
	dx11::setTexture(m_pTonemapEffect,"imageTexture",textureSRV);
	dx11::setTexture(m_pTonemapEffect,"imageTextureMS",textureSRV);
	hdrConstants.gamma				=Gamma;
	hdrConstants.exposure			=Exposure;
	
	float direction=(offsetX-0.5f)*2.0f;
	float x=offsetX/2.0f;
	float y=0,w=0.5f,h=1.0f;
    float as = float(640) / float(800);
	
	vec4 distortionK(1.0f,0.22f,0.24f,0.0f);
    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].

	float Distortion_XCenterOffset	=-direction*0.15197642f;
	float Distortion_Scale			=1.7146056f;
    float scaleFactor				=1.0f/Distortion_Scale;
	hdrConstants.warpHmdWarpParam	=distortionK;
	hdrConstants.warpLensCentre		=vec2(0.5f+Distortion_XCenterOffset*0.5f, 0.5f);
	hdrConstants.warpScreenCentre	=vec2(0.5f,0.5f);
	hdrConstants.warpScale			=vec2(0.5f* scaleFactor, 0.5f* scaleFactor * as);
	hdrConstants.warpScaleIn		=vec2(2.f,2.f/ as);
	hdrConstants.Apply(deviceContext);
	simul::dx11::setParameter(m_pTonemapEffect,"offset",offsetX,0.f);
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture_srv);
		simul::dx11::setTexture(m_pTonemapEffect,"glowTexture",glowTexture.g_pSRV_Output);
		ApplyPass(pContext,warpGlowExposureGamma->GetPassByIndex(0));
	}
	else
		ApplyPass(pContext,warpExposureGamma->GetPassByIndex(0));
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	
	dx11::setTexture(m_pTonemapEffect,"imageTexture",NULL);
	dx11::setTexture(m_pTonemapEffect,"imageTextureMS",NULL);
	hdrConstants.Unbind(pContext);
	ApplyPass(pContext,exposureGammaTechnique->GetPassByIndex(0));
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

void SimulHDRRendererDX1x::RenderGlowTexture(crossplatform::DeviceContext &deviceContext,void *texture_srv)
{
	if(!m_pGaussianEffect)
		return;
	ID3D11ShaderResourceView *textureSRV=(ID3D11ShaderResourceView*)texture_srv;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
static int g_NumApproxPasses=3;
static int	g_MaxApproxPasses = 8;
static float g_FilterRadius = 30;
	// Render to the low-res glow.
	if(glowTechnique)
	{
		dx11::setTexture(m_pTonemapEffect,"imageTexture",textureSRV);
		dx11::setTexture(m_pTonemapEffect,"imageTextureMS",textureSRV);
		simul::dx11::setParameter(m_pTonemapEffect,"offset",1.f/Width,1.f/Height);
		ApplyPass(pContext,glowTechnique->GetPassByIndex(0));
		glow_fb.Activate(pContext);
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
		glow_fb.Deactivate(pContext);
	}
    D3D11_TEXTURE2D_DESC tex_desc;
	ID3D1xTexture2D *texture=glow_fb.GetColorTexture();
	texture->GetDesc(&tex_desc);

	float box_width = CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width = box_width * 0.5f;
	float frac_half_box_width = (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width = 1.0f - frac_half_box_width;
	float rcp_box_width = 1.0f / box_width;
	// Step 1. Vertical passes: Each thread group handles a column in the image
	// Input texture
	simul::dx11::setTexture(m_pGaussianEffect,"g_texInput",(ID3D11ShaderResourceView*)glow_fb.GetColorTex());
	// Output texture
	simul::dx11::setUnorderedAccessView(m_pGaussianEffect,"g_rwtOutput",glowTexture.g_pUAV_Output);
	simul::dx11::setParameter(m_pGaussianEffect,"g_NumApproxPasses",g_NumApproxPasses - 1);
	simul::dx11::setParameter(m_pGaussianEffect,"g_HalfBoxFilterWidth",half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_FracHalfBoxFilterWidth",frac_half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_InvFracHalfBoxFilterWidth",inv_frac_half_box_width);
	simul::dx11::setParameter(m_pGaussianEffect,"g_RcpBoxFilterWidth",rcp_box_width);

	// Select pass
	gaussianColTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_col");
	gaussianColTechnique->GetPassByIndex(0)->Apply(0,pContext);
	pContext->Dispatch(tex_desc.Width,1,1);

	// Unbound CS resource and output
	ID3D11ShaderResourceView* srv_array[] = {NULL, NULL, NULL, NULL};
	pContext->CSSetShaderResources(0, 4, srv_array);
	ID3D11UnorderedAccessView* uav_array[] = {NULL, NULL, NULL, NULL};
	pContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);

	// Step 2. Horizontal passes: Each thread group handles a row in the image
	// Input texture
	simul::dx11::setTexture(m_pGaussianEffect,"g_texInput",(ID3D11ShaderResourceView*)glow_fb.GetColorTex());
	// Output texture
	simul::dx11::setUnorderedAccessView(m_pGaussianEffect,"g_rwtOutput",glowTexture.g_pUAV_Output);

	// Select pass
	gaussianRowTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_row");
	gaussianRowTechnique->GetPassByIndex(0)->Apply(0,pContext);
	pContext->Dispatch(tex_desc.Height,1,1);

	// Unbound CS resource and output
	pContext->CSSetShaderResources(0,4,srv_array);
	pContext->CSSetUnorderedAccessViews(0,4,uav_array, NULL);
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
