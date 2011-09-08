// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
#endif

#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
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

static D3DXMATRIX ident;
SimulWeatherRenderer::SimulWeatherRenderer(const char *lic,
			bool usebuffer,int width,
			int height,bool sky,bool clouds3d,
			bool clouds2d,bool rain,bool colour_sky) :
	BaseWeatherRenderer(lic),
	m_pBufferVertexDecl(NULL),
	m_pd3dDevice(NULL),
	m_pBufferToScreenEffect(NULL),
	use_buffer(usebuffer),
	exposure(1.f),
	gamma(1.f/2.2f),
	simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	simul2DCloudRenderer(NULL),
	simulPrecipitationRenderer(NULL),
	exposure_multiplier(1.f),
	show_rain(rain),
	renderDepthBufferCallback(NULL)
{
	D3DXMatrixIdentity(&ident);
	ShowSky=sky;
	if(sky)
	{
		simulSkyRenderer=new SimulSkyRenderer(colour_sky);
		baseSkyRenderer=simulSkyRenderer.get();
		AddChild(simulSkyRenderer.get());
	}
	EnableLayers(clouds3d,clouds2d);
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();
	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer;
	baseAtmosphericsRenderer=simulAtmosphericsRenderer.get();
	ConnectInterfaces();
	this->SetBufferSize(width,height);
}

void SimulWeatherRenderer::EnableLayers(bool clouds3d,bool clouds2d)
{
	BaseWeatherRenderer::EnableLayers(clouds3d,clouds2d);
	if(clouds3d&&simulCloudRenderer.get()==NULL)
	{
		simulCloudRenderer=new SimulCloudRenderer(license_key);
		baseCloudRenderer=simulCloudRenderer.get();
		AddChild(simulCloudRenderer.get());
		simulLightningRenderer=new SimulLightningRenderer(simulCloudRenderer->GetLightningRenderInterface());
		baseLightningRenderer=simulLightningRenderer.get();
		Restore3DCloudObjects();
	}
	if(clouds2d&&simul2DCloudRenderer.get()==NULL)
	{
		simul2DCloudRenderer=new Simul2DCloudRenderer(license_key);
		base2DCloudRenderer=simul2DCloudRenderer.get();
		Restore2DCloudObjects();
	}
	ConnectInterfaces();
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
bool SimulWeatherRenderer::Create(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	if(simulCloudRenderer)
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
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
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
			if(simulSkyRenderer)
				simulCloudRenderer->SetMaxFadeDistanceKm(simulSkyRenderer->GetMaxFadeDistanceKm());
			B_RETURN(simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
		}
		if(simulPrecipitationRenderer)
			B_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
		if(simulLightningRenderer)
			B_RETURN(simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	return (hr==S_OK);
}
bool SimulWeatherRenderer::Restore2DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simul2DCloudRenderer)
			B_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	}
	return (hr==S_OK);
}

bool SimulWeatherRenderer::RestoreDeviceObjects(void *dev)
{
	simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();

	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	if(!m_pBufferToScreenEffect)
		B_RETURN(CreateDX9Effect(m_pd3dDevice,m_pBufferToScreenEffect,"gamma.fx"));
	SkyOverStarsTechnique		=m_pBufferToScreenEffect->GetTechniqueByName("simul_sky_over_stars");
	CloudBlendTechnique			=m_pBufferToScreenEffect->GetTechniqueByName("simul_cloud_blend");
	bufferTexture				=m_pBufferToScreenEffect->GetParameterByName(NULL,"hdrTexture");
	B_RETURN(CreateBuffers());
	
	timer.UpdateTime();
	float create_buffers_time=timer.Time/1000.f;

	if(simulSkyRenderer)
		B_RETURN(simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice));
	timer.UpdateTime();
	float sky_restore_time=timer.Time/1000.f;
	B_RETURN(Restore3DCloudObjects());
	timer.UpdateTime();
	float clouds_3d_restore_time=timer.Time/1000.f;
	B_RETURN(Restore2DCloudObjects());
	timer.UpdateTime();
	float clouds_2d_restore_time=timer.Time/1000.f;
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);
	timer.UpdateTime();
	float atmospherics_restore_time=timer.Time/1000.f;
	std::cout<<std::setprecision(4)<<"RESTORE TIMINGS: create_buffers="<<create_buffers_time
		<<", sky="<<sky_restore_time<<", clouds_3d="<<clouds_3d_restore_time<<", clouds_2d="<<clouds_2d_restore_time
		<<", atmospherics="<<atmospherics_restore_time<<std::endl;

	UpdateSkyAndCloudHookup();
	return true;
}

bool SimulWeatherRenderer::InvalidateDeviceObjects()
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
	SAFE_RELEASE(m_pBufferVertexDecl);
	if(m_pBufferToScreenEffect)
        hr=m_pBufferToScreenEffect->OnLostDevice();
	SAFE_RELEASE(m_pBufferToScreenEffect);
	framebuffer.InvalidateDeviceObjects();
	lowdef_framebuffer.InvalidateDeviceObjects();
	return (hr==S_OK);
}

SimulWeatherRenderer::~SimulWeatherRenderer()
{
	InvalidateDeviceObjects();
	SAFE_DELETE_SMARTPTR(simulSkyRenderer);
	SAFE_DELETE_SMARTPTR(simulCloudRenderer);
	SAFE_DELETE_SMARTPTR(simul2DCloudRenderer);
	SAFE_DELETE_SMARTPTR(simulPrecipitationRenderer);
	SAFE_DELETE_SMARTPTR(simulAtmosphericsRenderer);
	ClearChildren();
}

void SimulWeatherRenderer::EnableRain(bool e)
{
	show_rain=e;
}

void SimulWeatherRenderer::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
	renderDepthBufferCallback=cb;
	//AlwaysRenderCloudsLate=(cb!=NULL);
}

void SimulWeatherRenderer::SetBufferSize(int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
}

bool SimulWeatherRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	framebuffer.SetWidthAndHeight(BufferWidth,BufferHeight);
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	lowdef_framebuffer.SetWidthAndHeight(BufferWidth/2,BufferHeight/2);
	lowdef_framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl);
	return (hr==S_OK);
}

bool SimulWeatherRenderer::RenderSky(bool buffered,bool is_cubemap)
{
	PIXBeginNamedEvent(0xFF888888,"SimulWeatherRenderer::Render");
	timer.TimeSum=0;
	timer.StartTime();
	BaseWeatherRenderer::RenderSky(buffered,is_cubemap);
	bool result=true;
	if(buffered&&!is_cubemap)
	{
		framebuffer.Activate();
		HRESULT hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,1.f,0L);
		result&=(hr==S_OK);
	}
	if(simulSkyRenderer)
	{
		if(ShowSky)
			simulSkyRenderer->Render(!buffered);
		// Do these updates now because sky renderer will have calculated the view height.
		if(simulAtmosphericsRenderer)
		{
			simulAtmosphericsRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
			simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
			simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			if(simulAtmosphericsRenderer)
			{
				simulAtmosphericsRenderer->SetLightningProperties(simulCloudRenderer->GetIlluminationTexture(),
																simulCloudRenderer->GetLightningRenderInterface());
			}
		}
	}
	timer.UpdateTime();
	simul::math::FirstOrderDecay(sky_timing,timer.Time,1.f,0.01f);
	if(simul2DCloudRenderer&&layer2)
		result&=simul2DCloudRenderer->Render(false,false,UseDefaultFog);
	if(simulCloudRenderer&&layer1&&(!RenderCloudsLate||is_cubemap))
	{
		for(int i=0;i<simulCloudRenderer->GetNumBuffers();i++)
		{
			//if(renderDepthBufferCallback)
			//	renderDepthBufferCallback->Render();
			// Disable writing to alpha for early cloud rendering:
			static int u=15;
			m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,u);
			RenderLateCloudLayer(i,i>0);
			m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);
			//hr=simulCloudRenderer->Render(is_cubemap);
		}
		timer.UpdateTime();
		simul::math::FirstOrderDecay(cloud_timing,timer.Time,1.f,0.01f);
	}
	else
		timer.UpdateTime();
	if(buffered&&!is_cubemap)
	{
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
		framebuffer.Deactivate();
		if(simulSkyRenderer&&ShowPlanets)
		{
			simulSkyRenderer->RenderPointStars();
		timer.UpdateTime();
			simulSkyRenderer->RenderSun();
			simulSkyRenderer->RenderPlanets();
		}
		m_pBufferToScreenEffect->SetTechnique(SkyOverStarsTechnique);
		// When we put the sky buffer to the screen we want to NOT change the destination alpha, because we may
		// have this set aside for depth!
		static int u=7;
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,u);
		RenderBufferToScreen(framebuffer.hdr_buffer_texture);
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);
	}
	timer.FinishTime();
	simul::math::FirstOrderDecay(total_timing,timer.TimeSum,1.f,0.01f);
	simul::math::FirstOrderDecay(final_timing,timer.Time,1.f,0.01f);
	return result;
}

bool SimulWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&layer1)
		return simulLightningRenderer->Render();
	return true;
}

bool SimulWeatherRenderer::RenderPrecipitation()
{
	if(simulPrecipitationRenderer&&layer1) 
		return simulPrecipitationRenderer->Render();
	return true;
}
bool SimulWeatherRenderer::RenderLateCloudLayer(bool buf)
{
	if(!RenderCloudsLate||!layer1)
		return true;
	timer.StartTime();
	bool result=true;
	for(int i=0;i<simulCloudRenderer->GetNumBuffers();i++)
	{
		result &=RenderLateCloudLayer(i,buf);
	}
	timer.FinishTime();
	simul::math::FirstOrderDecay(cloud_timing,timer.Time,1.f,0.01f);
	return result;
}

bool SimulWeatherRenderer::RenderLateCloudLayer(int buffer_index,bool buf)
{
	HRESULT hr=S_OK;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface=NULL;
	if(buf)
	{
		if(buffer_index==1)
			lowdef_framebuffer.Activate();
		else
			framebuffer.Activate();
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
	}
	//if(1)
	{
		//if(renderDepthBufferCallback)
		//	renderDepthBufferCallback->Render();
		if(simulCloudRenderer&&layer1)
		{	
			PIXWrapper(D3DCOLOR_RGBA(255,0,0,255),"CLOUDS")
			{
				hr=simulCloudRenderer->Render(false,false,false,buffer_index);
			}
		}
	}
	static bool do_fx=true;
	if(do_fx)
	if(simulAtmosphericsRenderer&&simulCloudRenderer&&layer1)
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
		if(str>0&&Godrays)
			simulAtmosphericsRenderer->RenderGodRays(str);
		simulAtmosphericsRenderer->RenderAirglow();
	}
	if(buf)
	{
		m_pBufferToScreenEffect->SetTechnique(CloudBlendTechnique);
		if(buffer_index==1)
		{
			lowdef_framebuffer.Deactivate();
	#ifdef XBOX
			m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL,lowdef_framebuffer.hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
	#endif
			RenderBufferToScreen(lowdef_framebuffer.hdr_buffer_texture);
		}
		else
		{
			framebuffer.Deactivate();
	#ifdef XBOX
			m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, framebuffer.hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
	#endif
			RenderBufferToScreen(framebuffer.hdr_buffer_texture);
		}
	}
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
	return (hr==S_OK);
}

bool SimulWeatherRenderer::RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture)
{
	HRESULT hr=m_pBufferToScreenEffect->SetTexture(bufferTexture,texture);
	hr=DrawFullScreenQuad(m_pd3dDevice,m_pBufferToScreenEffect);
	return (hr==S_OK);
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

void SimulWeatherRenderer::Update(float dt)
{
	BaseWeatherRenderer::Update(dt);
	if(simulCloudRenderer&&simulAtmosphericsRenderer)
	{
		LPDIRECT3DBASETEXTURE9 *c=(LPDIRECT3DBASETEXTURE9*)simulCloudRenderer->GetCloudTextures();
		simulAtmosphericsRenderer->SetCloudProperties(c[0],c[1],
			simulCloudRenderer->GetCloudScales(),
			simulCloudRenderer->GetCloudOffset(),
			simulCloudRenderer->GetInterpolation());
	}
	if(simulPrecipitationRenderer)
	{
		simulPrecipitationRenderer->Update(dt);
		if(layer1&&simulCloudRenderer)
		{
			simulPrecipitationRenderer->SetWind(simulCloudRenderer->GetWindSpeed(),simulCloudRenderer->GetWindHeadingDegrees());
			simulPrecipitationRenderer->SetIntensity(simulCloudRenderer->GetPrecipitationIntensity());
			float rts=simulCloudRenderer->GetRainToSnow();
			if(rts<0.5f)
				simulPrecipitationRenderer->ApplyDefaultRainSettings();
			else
				simulPrecipitationRenderer->ApplyDefaultSnowSettings();
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
	return simulSkyRenderer.get();
}

SimulCloudRenderer *SimulWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

Simul2DCloudRenderer *SimulWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

SimulPrecipitationRenderer *SimulWeatherRenderer::GetPrecipitationRenderer()
{
	return simulPrecipitationRenderer.get();
}

SimulAtmosphericsRenderer *SimulWeatherRenderer::GetAtmosphericsRenderer()
{
	return simulAtmosphericsRenderer.get();
}

float SimulWeatherRenderer::GetTiming() const
{
	return total_timing;
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
