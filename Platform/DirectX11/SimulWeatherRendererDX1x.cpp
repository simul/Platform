// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX1x.h"

#include <dxerr.h>
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "SimulSkyRendererDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "SimulPrecipitationRendererDX1x.h"

#include "SimulCloudRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulLightningRendererDX11.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"

D3DXMATRIX view_matrices[6];

SimulWeatherRendererDX1x::SimulWeatherRendererDX1x(simul::clouds::Environment *env,
		bool usebuffer,bool tonemap,int w,int h,bool sky,bool clouds3d,bool clouds2d,bool rain) :
	BaseWeatherRenderer(env,sky,rain),
	framebuffer(w/Downscale,h/Downscale),
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pTonemapEffect(NULL)
	,TonemapTechnique(NULL)
	,imageTexture(NULL)
	,worldViewProj(NULL)
	,simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	BufferWidth(w/Downscale),
	BufferHeight(h/Downscale),
	exposure_multiplier(1.f)
{
	simul::sky::SkyKeyframer *sk=env->skyKeyframer.get();
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer.get();
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer.get();
	if(ShowSky)
	{
		simulSkyRenderer=new SimulSkyRendererDX1x(sk);
		baseSkyRenderer=simulSkyRenderer.get();
		Group::AddChild(simulSkyRenderer.get());
	}
	simulCloudRenderer=new SimulCloudRendererDX1x(ck3d);
	simulLightningRenderer=new SimulLightningRendererDX11(ck3d,sk);
	baseCloudRenderer=simulCloudRenderer.get();
	Group::AddChild(simulCloudRenderer.get());
	if(clouds2d)
		base2DCloudRenderer=simul2DCloudRenderer=new Simul2DCloudRendererDX11(ck2d);
	if(rain)
		basePrecipitationRenderer=simulPrecipitationRenderer=new SimulPrecipitationRendererDX1x();

	baseAtmosphericsRenderer=simulAtmosphericsRenderer=new SimulAtmosphericsRendererDX1x;
	baseFramebuffer=&framebuffer;
	ConnectInterfaces();
}

void SimulWeatherRendererDX1x::SetScreenSize(int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
	
}

void SimulWeatherRendererDX1x::RestoreDeviceObjects(void* x)
{
	HRESULT hr=S_OK;
	void **u=(void**)x;
	m_pd3dDevice=(ID3D1xDevice*)u[0];
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);

	framebuffer_cubemap.SetWidthAndHeight(64,64);
	framebuffer_cubemap.RestoreDeviceObjects(m_pd3dDevice);

	pSwapChain=(IDXGISwapChain *)u[1];
// Get the back buffer (screen) format so we know how to render the weather buffer to the screen:
	ID3D1xTexture2D *pBackBuffer=NULL;
	pSwapChain->GetBuffer(0,__uuidof(ID3D1xTexture2D),(void**)&pBackBuffer);
	D3D1x_TEXTURE2D_DESC desc;
	pBackBuffer->GetDesc(&desc);
	SAFE_RELEASE(pBackBuffer);
	ScreenWidth=desc.Width;
	ScreenHeight=desc.Height;
	framebuffer.SetWidthAndHeight(BufferWidth,BufferHeight);
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);

	if(simulCloudRenderer)
	{
		simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
		if(simulSkyRenderer)
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulLightningRenderer)
		simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice);
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	}*/
	if(simulSkyRenderer)
	{
		simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice);
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	//V_RETURN(CreateBuffers());

	if(simul2DCloudRenderer)
		simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice);

	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void SimulWeatherRendererDX1x::RecompileShaders()
{
	BaseWeatherRenderer::RecompileShaders();
	framebuffer.RecompileShaders();
	SAFE_RELEASE(m_pTonemapEffect);
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,_T("simul_hdr.fx"));
	TonemapTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_direct");
	SkyOverStarsTechnique	=m_pTonemapEffect->GetTechniqueByName("simul_sky_over_stars");
	imageTexture				=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	worldViewProj			=m_pTonemapEffect->GetVariableByName("worldViewProj")->AsMatrix();
}

void SimulWeatherRendererDX1x::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pTonemapEffect);
	framebuffer.InvalidateDeviceObjects();
	framebuffer_cubemap.InvalidateDeviceObjects();
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
//	if(m_pTonemapEffect)
//        hr=m_pTonemapEffect->OnLostDevice();
// Free the cubemap resources. 
	SAFE_RELEASE(m_pImmediateContext);
}

bool SimulWeatherRendererDX1x::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();

	if(simulSkyRenderer.get())
		simulSkyRenderer->Destroy();
	Group::RemoveChild(simulSkyRenderer.get());
	simulSkyRenderer=NULL;

	if(simulCloudRenderer.get())
		simulCloudRenderer->Destroy();
	Group::RemoveChild(simulCloudRenderer.get());
	simulCloudRenderer=NULL;

	Group::RemoveChild(simulAtmosphericsRenderer.get());
	simulAtmosphericsRenderer=NULL;
	/*if(simul2DCloudRenderer)
		simul2DCloudRenderer->Destroy();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->Destroy();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();*/
	return (hr==S_OK);
}

SimulWeatherRendererDX1x::~SimulWeatherRendererDX1x()
{
	InvalidateDeviceObjects();
	Destroy();
/*	SAFE_DELETE(simul2DCloudRenderer);
	SAFE_DELETE(simulPrecipitationRenderer);
	SAFE_DELETE(simulAtmosphericsRenderer);*/
}

/*bool SimulWeatherRendererDX1x::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}
*/
static D3DXVECTOR3 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR3 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

void SimulWeatherRendererDX1x::SaveCubemapToFile(const char *filename)
{
	FramebufferCubemapDX1x	fb_cubemap;
	fb_cubemap.SetWidthAndHeight(1024,1024);
	fb_cubemap.RestoreDeviceObjects(m_pd3dDevice);

	cam_pos=GetCameraPosVector(view);
	MakeCubeMatrices(view_matrices,cam_pos);
	for(int i=0;i<6;i++)
	{
		fb_cubemap.SetCurrentFace(i);
		fb_cubemap.Activate();
		if(simulSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovRH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				600000.f);
			SetMatrices(view_matrices[i],cube_proj);
			HRESULT hr=RenderSky(false,true);
		}
		fb_cubemap.Deactivate();
	}
	std::wstring wstr=simul::base::StringToWString(filename);
	ID3D11Texture2D *tex=fb_cubemap.GetCopy();
	HRESULT hr=D3DX11SaveTextureToFile(m_pImmediateContext,tex,D3DX11_IFF_DDS,wstr.c_str());
}

bool SimulWeatherRendererDX1x::RenderCubemap()
{
	D3DXMATRIX ov=view;
	D3DXMATRIX op=proj;
	cam_pos=GetCameraPosVector(view);
	MakeCubeMatrices(view_matrices,cam_pos);
	for(int i=0;i<6;i++)
	{
		framebuffer_cubemap.SetCurrentFace(i);
		framebuffer_cubemap.Activate();
		if(simulSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovRH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				200000.f);
			SetMatrices(view_matrices[i],cube_proj);
			HRESULT hr=RenderSky(false,true);
		}
		framebuffer_cubemap.Deactivate();
	}
	SetMatrices(ov,op);
	return true;
}

void *SimulWeatherRendererDX1x::GetCubemap()
{
	return framebuffer_cubemap.GetColorTex();// m_pCubeEnvMapSRV;
}

bool SimulWeatherRendererDX1x::RenderSky(bool buffered,bool is_cubemap)
{
	simul::clouds::BaseWeatherRenderer::RenderSky(buffered,is_cubemap);
	HRESULT hr=S_OK;
	if(buffered&&baseFramebuffer)
	{
		bool blend=!is_cubemap;
		HRESULT hr=S_OK;
		hr=imageTexture->SetResource(framebuffer.buffer_texture_SRV);
		ID3D1xEffectTechnique *tech=blend?SkyOverStarsTechnique:TonemapTechnique;
		ApplyPass(tech->GetPassByIndex(0));
		
		D3DXMATRIX ortho;
		D3DXMatrixIdentity(&ortho);
		D3DXMatrixOrthoLH(&ortho,2.f,2.f,-100.f,100.f);
		worldViewProj->SetMatrix(ortho);
		
		framebuffer.DrawQuad();
		imageTexture->SetResource(NULL);
	}
	return (hr==S_OK);
}

void SimulWeatherRendererDX1x::RenderLateCloudLayer(bool )
{
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		simulCloudRenderer->Render(false,depth_alpha_tex,UseDefaultFog,true);
	}
}

void SimulWeatherRendererDX1x::RenderPrecipitation()
{
	if(simulPrecipitationRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render();
}

void SimulWeatherRendererDX1x::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render();
}

void SimulWeatherRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(view,proj);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(view,proj);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(view,proj);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(view,proj);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(view,proj);
	if(simulLightningRenderer)
		simulLightningRenderer->SetMatrices(view,proj);
}

void SimulWeatherRendererDX1x::UpdateSkyAndCloudHookup()
{
	simul::clouds::BaseWeatherRenderer::UpdateSkyAndCloudHookup();
	if(!simulSkyRenderer)
		return;
	void *l=0,*i=0,*s=0;
	simulSkyRenderer->Get2DLossAndInscatterTextures(&l,&i,&s);
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetLossTexture(l);
		simulCloudRenderer->SetInscatterTextures(i,s);
	}
	
}
void SimulWeatherRendererDX1x::Update()
{
	BaseWeatherRenderer::Update();
	static bool pause=false;
    if(!pause)
	{
		if(simulSkyRenderer)
		{
			if(simulCloudRenderer)
			{
				/*simulCloudRenderer->SetLossTexture(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulCloudRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());*/
				simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
				simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
			}
		/*	if(simulAtmosphericsRenderer)
			{
				if(simulCloudRenderer)
					simulAtmosphericsRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
				simulAtmosphericsRenderer->SetLossTexture(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulAtmosphericsRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());
				simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			}*/
		}
/*		if(simulCloudRenderer)
		{
			simulCloudRenderer->Update(dt);
		}
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->Update(dt);*/
	}
}

SimulSkyRendererDX1x *SimulWeatherRendererDX1x::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

SimulCloudRendererDX1x *SimulWeatherRendererDX1x::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

Simul2DCloudRendererDX11 *SimulWeatherRendererDX1x::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

const char *SimulWeatherRendererDX1x::GetDebugText() const
{
	static char debug_text[256];
	if(simulSkyRenderer)
		sprintf_s(debug_text,256,"%3.3g",simulSkyRenderer->GetSkyKeyframer()->GetInterpolation());
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
//	if(simulCloudRenderer)
	sprintf_s(debug_text,256,"TIME %2.2g ms\nClouds %2.2g update %2.2g\nSky %2.2g update %2.2g",total_timing,
			baseCloudRenderer?baseCloudRenderer->GetRenderTime():0.f,environment->cloud_update_timing,sky_timing,environment->sky_update_timing);
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
	return debug_text;
}
//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
void SimulWeatherRendererDX1x::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
}