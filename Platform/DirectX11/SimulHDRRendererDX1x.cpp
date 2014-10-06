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
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "MacrosDX1x.h"
#include "Utilities.h"
#include "Simul/Math/Pi.h"
#include "D3dx11effect.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

using namespace simul;
using namespace dx11;

SimulHDRRendererDX1x::SimulHDRRendererDX1x(int w,int h)
	:m_pd3dDevice(NULL)
	,renderPlatform(NULL)
	,hdr_effect(NULL)
	,m_pGaussianEffect(NULL)
	,Glow(false)
	,Width(0)
	,Height(0)
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
		glowTexture.ensureTexture2DSizeAndFormat(renderPlatform,Width/2,Height/2,crossplatform::R_32_UINT,true);
		
		glow_fb.SetFormat(crossplatform::RGBA_16_FLOAT);
		glow_fb.SetWidthAndHeight(Width/2,Height/2);
	}
	RecompileShaders();
}

void SimulHDRRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HRESULT hr=S_OK;
	renderPlatform=r;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	glow_fb.RestoreDeviceObjects(renderPlatform);

	if(m_pd3dDevice&&Width>0&&Height>0)
	{
		glowTexture.ensureTexture2DSizeAndFormat(renderPlatform,Width/2,Height/2,crossplatform::R_32_UINT,true);
	}
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	imageConstants.RestoreDeviceObjects(renderPlatform);
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
	static int	threadsPerGroup = 128;

void SimulHDRRendererDX1x::RecompileShaders()
{
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defs;
	hdr_effect					=renderPlatform->CreateEffect("hdr",defs);
	exposureGammaTechnique		=hdr_effect->GetTechniqueByName("exposure_gamma");
	glowExposureGammaTechnique	=hdr_effect->GetTechniqueByName("glow_exposure_gamma");
	warpExposureGamma			=hdr_effect->GetTechniqueByName("warp_exposure_gamma");
	warpGlowExposureGamma		=hdr_effect->GetTechniqueByName("warp_glow_exposure_gamma");
	glowTechnique				=hdr_effect->GetTechniqueByName("simul_glow");
	hdrConstants.LinkToEffect(hdr_effect,"HdrConstants");
	

//static const uint THREADS_PER_GROUP = 128;
//static const uint SCAN_SMEM_SIZE = 1200;
//static const uint TEXELS_PER_THREAD = 5;
//static const uint NUM_IMAGE_ROWS = 1200;
//static const uint NUM_IMAGE_COLS = 1600;
	int W=Width;
	int H=Height;
	if(!H||!W)
		return;
	// Just set scan_mem_size to the largest value we can ever expect:
	int scan_smem_size			=1920;//max3(H,W,(int)threadsPerGroup*2);//1920;//
	defs["SCAN_SMEM_SIZE"]		=string_format("%d",scan_smem_size);
	defs["THREADS_PER_GROUP"]	=string_format("%d",threadsPerGroup);

	//int texels_per_thread		=(std::max(H,W) + threadsPerGroup - 1) / threadsPerGroup;
	//defs["TEXELS_PER_THREAD"]	=string_format("%d",texels_per_thread);
	//defs["NUM_IMAGE_COLS"]		=string_format("%d",W);
	//defs["NUM_IMAGE_ROWS"]		=string_format("%d",H);
	
	m_pGaussianEffect			=renderPlatform->CreateEffect("simul_gaussian",defs);
	hdrConstants.LinkToEffect(m_pGaussianEffect,"HdrConstants");
	imageConstants.LinkToEffect(m_pGaussianEffect,"ImageConstants");
}

void SimulHDRRendererDX1x::InvalidateDeviceObjects()
{
	hdrConstants.InvalidateDeviceObjects();
	imageConstants.InvalidateDeviceObjects();
	glow_fb.InvalidateDeviceObjects();
	SAFE_DELETE(hdr_effect);
	SAFE_DELETE(m_pGaussianEffect);
	glowTexture.InvalidateDeviceObjects();
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

void SimulHDRRendererDX1x::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float Exposure,float Gamma)
{
	Render(deviceContext,texture,0,Exposure, Gamma);
}

void SimulHDRRendererDX1x::Render(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,float offsetX,float Exposure,float Gamma)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"HDR")
	bool msaa=(texture->GetSampleCount()>1);
	if(msaa)
		hdr_effect->SetTexture(deviceContext,"imageTextureMS"	,texture);
	else
		hdr_effect->SetTexture(deviceContext,"imageTexture"	,texture);
	hdrConstants.gamma		=Gamma;
	hdrConstants.exposure	=Exposure;
	hdrConstants.Apply(deviceContext);
	simul::dx11::setParameter(hdr_effect->asD3DX11Effect(),"offset",offsetX,0.f);
	crossplatform::EffectTechnique *tech=exposureGammaTechnique;
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture);
		tech=glowExposureGammaTechnique;
		simul::dx11::setTexture(hdr_effect->asD3DX11Effect(),"glowTexture",glowTexture.AsD3D11ShaderResourceView());
	}
	hdr_effect->Apply(deviceContext,tech,(msaa?"msaa":"main"));
	simul::dx11::UtilityRenderer::DrawQuad(deviceContext);

	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTexture",NULL);
	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	imageConstants.Unbind(deviceContext);
	
	hdr_effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void SimulHDRRendererDX1x::RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,void *texture_srv
	,float offsetX,float Exposure,float Gamma)
{
	ID3D11DeviceContext *pContext		=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	ID3D11ShaderResourceView *textureSRV=(ID3D11ShaderResourceView*)texture_srv;
	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTexture",textureSRV);
	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTextureMS",textureSRV);
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
	simul::dx11::setParameter(hdr_effect->asD3DX11Effect(),"offset",offsetX,0.f);
	if(Glow)
	{
		RenderGlowTexture(deviceContext,texture_srv);
		simul::dx11::setTexture(hdr_effect->asD3DX11Effect(),"glowTexture",glowTexture.AsD3D11ShaderResourceView());
		hdr_effect->Apply(deviceContext,warpGlowExposureGamma,0);
	}
	else
		hdr_effect->Apply(deviceContext,warpExposureGamma,0);
	simul::dx11::UtilityRenderer::DrawQuad(deviceContext);
	
	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTexture",NULL);
	dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTextureMS",NULL);
	hdrConstants.Unbind(deviceContext);
	hdr_effect->Unapply(deviceContext);
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
//	if(!m_pGaussianEffect)
		return;
	ID3D11ShaderResourceView *textureSRV=(ID3D11ShaderResourceView*)texture_srv;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	static int g_NumApproxPasses=3;
	static int	g_MaxApproxPasses = 8;
	static float g_FilterRadius = 30;
	// Render to the low-res glow.
	if(glowTechnique)
	{
		dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTexture",textureSRV);
		dx11::setTexture(hdr_effect->asD3DX11Effect(),"imageTextureMS",textureSRV);
		simul::dx11::setParameter(hdr_effect->asD3DX11Effect(),"offset",1.f/Width,1.f/Height);
		hdr_effect->Apply(deviceContext,glowTechnique,(0));
		glow_fb.Activate(deviceContext);
		glow_fb.Clear(deviceContext, 0, 0, 0, 0, 0);
		simul::dx11::UtilityRenderer::DrawQuad(deviceContext);
		glow_fb.Deactivate(deviceContext);
		hdr_effect->Unapply(deviceContext);
	}
    D3D11_TEXTURE2D_DESC tex_desc;
	ID3D1xTexture2D *texture=glow_fb.GetColorTexture();
	texture->GetDesc(&tex_desc);

	float box_width					= CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width			= box_width * 0.5f;
	float frac_half_box_width		= (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width	= 1.0f - frac_half_box_width;
	float rcp_box_width				= 1.0f / box_width;
	// Step 1. Vertical passes: Each thread group handles a column in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",glow_fb.GetTexture());
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",&glowTexture);
	imageConstants.imageSize					=uint2(tex_desc.Width,tex_desc.Height);
	// Each thread is a chunk of threadsPerGroup(=128) texels, so to cover all of them we divide by threadsPerGroup
	imageConstants.texelsPerThread				=(tex_desc.Height + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.g_NumApproxPasses			=g_NumApproxPasses-1;
	imageConstants.g_HalfBoxFilterWidth			=half_box_width;
	imageConstants.g_FracHalfBoxFilterWidth		=frac_half_box_width;
	imageConstants.g_InvFracHalfBoxFilterWidth	=inv_frac_half_box_width;
	imageConstants.g_RcpBoxFilterWidth			=rcp_box_width;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianColTechnique						=m_pGaussianEffect->GetTechniqueByName("simul_gaussian_col");
	m_pGaussianEffect->Apply(deviceContext,gaussianColTechnique,0);
	// We perform the Gaussian blur for each column. Each group is a column, and each thread 
	pContext->Dispatch(tex_desc.Width,1,1);

	// Unbound CS resource and output
	ID3D11ShaderResourceView* srv_array[] = {NULL, NULL, NULL, NULL};
	pContext->CSSetShaderResources(0, 4, srv_array);
	ID3D11UnorderedAccessView* uav_array[] = {NULL, NULL, NULL, NULL};
	pContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);
	
	m_pGaussianEffect->Unapply(deviceContext);
	// Step 2. Horizontal passes: Each thread group handles a row in the image
	// Input texture
	m_pGaussianEffect->SetTexture(deviceContext,"g_texInput",glow_fb.GetTexture());
	// Output texture
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",&glowTexture);
	imageConstants.texelsPerThread				=(tex_desc.Width + threadsPerGroup - 1)/threadsPerGroup;
	imageConstants.Apply(deviceContext);
	// Select pass
	gaussianRowTechnique = m_pGaussianEffect->GetTechniqueByName("simul_gaussian_row");
	m_pGaussianEffect->Apply(deviceContext,gaussianRowTechnique,0);
	pContext->Dispatch(tex_desc.Height,1,1);

	// Unbound CS resource and output
	pContext->CSSetShaderResources(0,4,srv_array);
	pContext->CSSetUnorderedAccessViews(0,4,uav_array, NULL);
	m_pGaussianEffect->SetUnorderedAccessView(deviceContext,"g_rwtOutput",NULL);
	m_pGaussianEffect->Unapply(deviceContext);
}

void SimulHDRRendererDX1x::RenderDebug(crossplatform::DeviceContext &deviceContext,int x0,int y0,int w,int h)
{
	renderPlatform->DrawTexture(deviceContext,x0,y0,w/2,h/2,glow_fb.GetTexture());
	//renderPlatform->DrawTexture(deviceContext,x0+w/2,y0,w/2,h/2,&glowTexture);
	simul::dx11::setTexture(hdr_effect->asD3DX11Effect(),"glowTexture",glowTexture.AsD3D11ShaderResourceView());
	renderPlatform->DrawQuad(deviceContext,x0+w/2,y0,w/2,h/2,hdr_effect,hdr_effect->GetTechniqueByName("show_compressed_texture"));
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
