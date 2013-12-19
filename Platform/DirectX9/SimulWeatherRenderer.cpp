#define NOMINMAX
// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRenderer.h"

#include <tchar.h>
#include <dxerr.h>
#include <string>

#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
#include "Simul/Math/Decay.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Environment.h"
#include "Simul/Platform/DirectX9/SimulCloudRenderer.h"
#include "Simul/Platform/DirectX9/SimulLightningRenderer.h"
#include "Simul/Platform/DirectX9/SimulPrecipitationRenderer.h"
#include "Simul/Platform/DirectX9/Simul2DCloudRenderer.h"
#include "Simul/Platform/DirectX9/SimulSkyRenderer.h"
#include "Simul/Platform/DirectX9/SimulAtmosphericsRenderer.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/Resources.h"
#include <iomanip>

#define WRITE_PERFORMANCE_DATA
static simul::base::Timer timer;
using namespace simul;
using namespace dx9;

TwoResFramebuffer::TwoResFramebuffer()
	:m_pd3dDevice(NULL)
	,Width(0)
	,Height(0)
	,Downscale(0)
{
}

void TwoResFramebuffer::RestoreDeviceObjects(void *dev)
{
	if(!dev)
		return;
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	D3DFORMAT INTZ=((D3DFORMAT) MAKEFOURCC('I','N','T','Z'));
	lowResFarFramebufferDx11	.SetDepthFormat(INTZ);
	lowResNearFramebufferDx11	.SetDepthFormat(0);
	hiResFarFramebufferDx11		.SetDepthFormat(0);
	hiResNearFramebufferDx11	.SetDepthFormat(0);

	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth		=(Width+Downscale-1)/Downscale;
	int BufferHeight	=(Height+Downscale-1)/Downscale;
	lowResFarFramebufferDx11	.SetWidthAndHeight(BufferWidth,BufferHeight);
	lowResNearFramebufferDx11	.SetWidthAndHeight(BufferWidth,BufferHeight);
	hiResFarFramebufferDx11		.SetWidthAndHeight(Width,Height);
	hiResNearFramebufferDx11	.SetWidthAndHeight(Width,Height);
	
	lowResFarFramebufferDx11	.RestoreDeviceObjects(dev);
	lowResNearFramebufferDx11	.RestoreDeviceObjects(dev);
	hiResFarFramebufferDx11		.RestoreDeviceObjects(dev);
	hiResNearFramebufferDx11	.RestoreDeviceObjects(dev);
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
}

void TwoResFramebuffer::SetDimensions(int w,int h,int downscale)
{
	if(Width!=w||Height!=h||Downscale!=downscale)
	{
		Width=w;
		Height=h;
		Downscale=downscale;
		RestoreDeviceObjects(m_pd3dDevice);
	}
}


SimulWeatherRenderer::SimulWeatherRenderer(	simul::clouds::Environment *env,
										   simul::base::MemoryInterface *mem,
											bool usebuffer,int width,
											int height,bool sky,bool rain)
	:BaseWeatherRenderer(env,mem)
	,m_pd3dDevice(NULL)
	,m_pBufferToScreenEffect(NULL)
	,use_buffer(usebuffer)
	,exposure(1.f)
	,gamma(1.f/2.2f)
	,simulSkyRenderer(NULL)
	,simulCloudRenderer(NULL)
	,simulLightningRenderer(NULL)
	,simul2DCloudRenderer(NULL)
	,simulPrecipitationRenderer(NULL)
	,simulAtmosphericsRenderer(NULL)
	,exposure_multiplier(1.f)
	,show_rain(rain)
{
	simul::sky::SkyKeyframer *sk=env->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer;
	SetScreenSize(0,width,height);
	
		simulSkyRenderer=new SimulSkyRenderer(sk);
		baseSkyRenderer=simulSkyRenderer;
#if 1
	{
		simulCloudRenderer=new SimulCloudRenderer(ck3d,mem);
		baseCloudRenderer=simulCloudRenderer;
	}
	/*
	{
		simulLightningRenderer=new SimulLightningRenderer(ck3d,sk);
		baseLightningRenderer=simulLightningRenderer;
		Restore3DCloudObjects();
	}
	
	{
		simul2DCloudRenderer=new Simul2DCloudRenderer(ck2d,mem);
		base2DCloudRenderer=simul2DCloudRenderer;
		Restore2DCloudObjects();
	}
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();*/
	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer(mem);
	baseAtmosphericsRenderer=simulAtmosphericsRenderer;
#endif
	framebuffers[0]=new TwoResFramebuffer();
	ConnectInterfaces();
}

clouds::TwoResFramebuffer *SimulWeatherRenderer::GetFramebuffer(int view_id)
{
	if(framebuffers.find(view_id)==framebuffers.end())
	{
		dx9::TwoResFramebuffer *fb=new dx9::TwoResFramebuffer();
		framebuffers[view_id]=fb;
		fb->RestoreDeviceObjects(m_pd3dDevice);
		return fb;
	}
	return framebuffers[view_id];
}


/*
void SimulWeatherRenderer::ConnectInterfaces()
{
	if(!simulSkyRenderer)
		return;
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
}
*/
void SimulWeatherRenderer::SetScreenSize(int view_id,int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
}


bool SimulWeatherRenderer::Create(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
/*	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simul2DCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulSkyRenderer)
	{
		if(simulCloudRenderer)
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
	if(simulCloudRenderer&&simulSkyRenderer)
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());*/
	HRESULT hr=S_OK;
	return (hr==S_OK);
}


bool SimulWeatherRenderer::Restore3DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simulCloudRenderer)
		{
			simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
		}
		if(simulPrecipitationRenderer)
			simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice);
		if(simulLightningRenderer)
			simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice);
	}
	return (hr==S_OK);
}
bool SimulWeatherRenderer::Restore2DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
	}
	return (hr==S_OK);
}

void SimulWeatherRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	BaseWeatherRenderer::RecompileShaders();
	SAFE_RELEASE(m_pBufferToScreenEffect);
	V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pBufferToScreenEffect,"gamma.fx"));
	SkyOverStarsTechnique		=m_pBufferToScreenEffect->GetTechniqueByName("simul_sky_over_stars");
	CloudBlendTechnique			=m_pBufferToScreenEffect->GetTechniqueByName("cloud_blend");
	bufferTexture				=m_pBufferToScreenEffect->GetParameterByName(NULL,"hdrTexture");
}

void SimulWeatherRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	RecompileShaders();
	CreateBuffers();
	if(simulSkyRenderer)
		simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice);
	Restore3DCloudObjects();
	Restore2DCloudObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);
	UpdateSkyAndCloudHookup();
}

void SimulWeatherRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	if(m_pBufferToScreenEffect)
        hr=m_pBufferToScreenEffect->OnLostDevice();
	SAFE_RELEASE(m_pBufferToScreenEffect);
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
		i->second->InvalidateDeviceObjects();
}

SimulWeatherRenderer::~SimulWeatherRenderer()
{
	InvalidateDeviceObjects();
	del(simulSkyRenderer,memoryInterface);
	del(simulCloudRenderer,memoryInterface);
	del(simul2DCloudRenderer,memoryInterface);
	del(simulPrecipitationRenderer,memoryInterface);
	del(simulAtmosphericsRenderer,memoryInterface);
}

void SimulWeatherRenderer::EnableRain(bool e)
{
	show_rain=e;
}

bool SimulWeatherRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
	{
		i->second->SetDimensions(BufferWidth,BufferHeight,Downscale);
	
		i->second->RestoreDeviceObjects(m_pd3dDevice);
	}
	return (hr==S_OK);
}

void SimulWeatherRenderer::RenderSkyAsOverlay(void *context,
												float exposure,
												bool buffered,
												bool is_cubemap,
												const void* mainDepthTexture,
												const void* depthTextureForClouds,
												int viewport_id,
												const simul::sky::float4& relativeViewportTextureRegionXYWH,
												bool doFinalCloudBufferToScreenComposite
												)
{
	SIMUL_COMBINED_PROFILE_START(context,"RenderSkyAsOverlay")
	BaseWeatherRenderer::RenderSkyAsOverlay(context,
											exposure,
											buffered,
											is_cubemap,
											mainDepthTexture,
											depthTextureForClouds,
											viewport_id,
											relativeViewportTextureRegionXYWH,
											doFinalCloudBufferToScreenComposite
											);
	if(buffered&&doFinalCloudBufferToScreenComposite)
	{
		clouds::TwoResFramebuffer *fb=GetFramebuffer(viewport_id);
		m_pBufferToScreenEffect->SetTexture(bufferTexture,(LPDIRECT3DBASETEXTURE9)fb->GetLowResFarFramebuffer()->GetColorTex());
		m_pBufferToScreenEffect->SetTechnique(CloudBlendTechnique);
		unsigned passes;
		simul::dx9::setParameter(m_pBufferToScreenEffect,"exposure",exposure);
		//simul::dx9::setParameter(m_pBufferToScreenEffect,"gamma",1.f);
		m_pBufferToScreenEffect->Begin(&passes,0);
		m_pBufferToScreenEffect->BeginPass(0);

		simul::dx9::DrawQuad(m_pd3dDevice);
		m_pBufferToScreenEffect->EndPass();
		m_pBufferToScreenEffect->End();
		m_pBufferToScreenEffect->SetTexture(bufferTexture,NULL);
	}
	SIMUL_COMBINED_PROFILE_END(context)
}


void SimulWeatherRenderer::RenderLightning(void *context,int viewport_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return simulLightningRenderer->Render(context);
}

void SimulWeatherRenderer::RenderPrecipitation(void *context)
{
	if(simulPrecipitationRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}

void SimulWeatherRenderer::RenderLateCloudLayer(void *context,float exposure,bool buf,int viewport_id,const simul::sky::float4 &relativeViewportTextureRegionXYWH)
{
	if(!RenderCloudsLate||!simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return;
	HRESULT hr=S_OK;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface=NULL;
		clouds::TwoResFramebuffer *fb=GetFramebuffer(viewport_id);
	if(buf)
	{
		fb->GetLowResFarFramebuffer()->Activate(NULL);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
	}
	
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{	
		PIXWrapper(D3DCOLOR_RGBA(255,0,0,255),"CLOUDS")
		{
			simulCloudRenderer->Render(context,exposure,false,false,0,false,true,viewport_id,relativeViewportTextureRegionXYWH);
		}
	}
	
	static bool do_fx=true;
	if(do_fx)
	if(simulAtmosphericsRenderer&&simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		float str=simulCloudRenderer->GetCloudInterface()->GetHumidity();
		static float gr_start=0.65f;
		static float gr_strength=0.5f;
		str-=gr_start;
		str/=(1.f-gr_start);
		if(str<0.f)
			str=0.f;
		if(str>1.f)
			str=1.f;
		str*=gr_strength;
		if(str>0&&simulAtmosphericsRenderer->GetShowGodrays())
			simulAtmosphericsRenderer->RenderGodRays(str);
		simulAtmosphericsRenderer->RenderAirglow();
	}
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}
#ifdef XBOX
void SimulWeatherRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(v,p);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(v,p);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(v,p);
}
#endif

void SimulWeatherRenderer::PreRenderUpdate(void *context,float dt)
{
	BaseWeatherRenderer::PreRenderUpdate(context);
	//if(baseCloudRenderer&&baseAtmosphericsRenderer)
	{
		//void **c=baseCloudRenderer->GetCloudTextures();
	/*	baseAtmosphericsRenderer->SetCloudProperties(c[0],c[1],
			baseCloudRenderer->GetCloudScales(),
			baseCloudRenderer->GetCloudOffset(),
			baseCloudRenderer->GetInterpolation());*/
	}
	if(simulPrecipitationRenderer)
	{
		simulPrecipitationRenderer->PreRenderUpdate(context,dt);
		if(simulCloudRenderer&&environment->cloudKeyframer->GetVisible())
		{
			simulPrecipitationRenderer->SetWind(environment->cloudKeyframer->GetWindSpeed(),environment->cloudKeyframer->GetWindHeadingDegrees());
		#ifndef XBOX
			float cam_pos[3];
			D3DXMATRIX view;
			m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		#endif
			GetCameraPosVector(view,simulCloudRenderer->IsYVertical(),cam_pos);
			simulPrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
		}
		else
		{
			simulPrecipitationRenderer->SetIntensity(0.f);
			simulPrecipitationRenderer->SetWind(0,0);
		}
		if(simulSkyRenderer)
		{
			simul::sky::float4 l=simulSkyRenderer->GetLightColour();
			simulPrecipitationRenderer->SetLightColour((const float*)(l));
		}
	}
}

SimulSkyRenderer *SimulWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer;
}

SimulCloudRenderer *SimulWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer;
}

Simul2DCloudRenderer *SimulWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}

SimulPrecipitationRenderer *SimulWeatherRenderer::GetPrecipitationRenderer()
{
	return simulPrecipitationRenderer;
}

SimulAtmosphericsRenderer *SimulWeatherRenderer::GetAtmosphericsRenderer()
{
	return simulAtmosphericsRenderer;
}
float SimulWeatherRenderer::GetTotalBrightness() const
{
	return exposure*exposure_multiplier;
} 

const char *SimulWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	if(simulSkyRenderer)
		sprintf_s(debug_text,256,"%s",simulSkyRenderer->GetDebugText());
/*	if(simulCloudRenderer)
		sprintf_s(debug_text,256,"%s\ntotal %3.3g ms, clouds %3.3g ms, sky %3.3g ms, final %3.3g",
			simulCloudRenderer->GetDebugText(),total_timing,cloud_timing,sky_timing,final_timing);*/
	return debug_text;
}
