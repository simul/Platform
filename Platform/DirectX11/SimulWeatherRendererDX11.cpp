#define NOMINMAX
// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX11.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX11.h"

#include <dxerr.h>
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Camera/Camera.h"
#include "SimulSkyRendererDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "SimulPrecipitationRendererDX1x.h"

#include "SimulCloudRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulLightningRendererDX11.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"

using namespace simul;
using namespace dx11;

SimulWeatherRendererDX11::SimulWeatherRendererDX11(simul::clouds::Environment *env
													,simul::base::MemoryInterface *mem) :
	BaseWeatherRenderer(env,mem),
	framebuffer(0,0),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL)
	,directTechnique(NULL)
	,imageTexture(NULL)
	,simulSkyRenderer(NULL)
	,simulCloudRenderer(NULL)
	,simulPrecipitationRenderer(NULL)
	,simulAtmosphericsRenderer(NULL)
	,simul2DCloudRenderer(NULL)
	,simulLightningRenderer(NULL)
	,BufferWidth(0)
	,BufferHeight(0)
	,exposure_multiplier(1.f)
	,memoryInterface(mem)
{
	simul::sky::SkyKeyframer *sk=env->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer;
	//if(ShowSky)
	{
		simulSkyRenderer	=::new(memoryInterface) SimulSkyRendererDX1x(sk, memoryInterface);
		baseSkyRenderer		=simulSkyRenderer;
	}
	simulCloudRenderer		=::new(memoryInterface) SimulCloudRendererDX1x(ck3d, memoryInterface);
	baseCloudRenderer		=simulCloudRenderer;
	simulLightningRenderer	=::new(memoryInterface) SimulLightningRendererDX11(ck3d,sk);
	if(env->cloud2DKeyframer)
		base2DCloudRenderer=simul2DCloudRenderer		=::new(memoryInterface) Simul2DCloudRendererDX11(ck2d, memoryInterface);
	basePrecipitationRenderer=simulPrecipitationRenderer=::new(memoryInterface) SimulPrecipitationRendererDX1x();
	baseAtmosphericsRenderer=simulAtmosphericsRenderer	=::new(memoryInterface) SimulAtmosphericsRendererDX1x(mem);
	baseFramebuffer=&framebuffer;
	framebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	ConnectInterfaces();
}

void SimulWeatherRendererDX11::SetScreenSize(int w,int h)
{
	ScreenWidth		=w;
	ScreenHeight	=h;
	BufferSizeChanged();
}

void SimulWeatherRendererDX11::BufferSizeChanged()
{
	BufferWidth		=ScreenWidth/Downscale;
	BufferHeight	=ScreenHeight/Downscale;
	framebuffer.SetWidthAndHeight(BufferWidth,BufferHeight);
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	hdrConstants.RestoreDeviceObjects(m_pd3dDevice);
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

void SimulWeatherRendererDX11::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(m_pTonemapEffect);
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	CreateEffect(m_pd3dDevice,&m_pTonemapEffect,("simul_hdr.fx"), defines);
	directTechnique		=m_pTonemapEffect->GetTechniqueByName("simul_direct");
	showDepthTechnique	=m_pTonemapEffect->GetTechniqueByName("show_depth");
	SkyBlendTechnique	=m_pTonemapEffect->GetTechniqueByName("simul_sky_blend");
	imageTexture		=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	hdrConstants.LinkToEffect(m_pTonemapEffect,"HdrConstants");
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pTonemapEffect);
	framebuffer.InvalidateDeviceObjects();
	hdrConstants.InvalidateDeviceObjects();
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
}

bool SimulWeatherRendererDX11::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();

	if(simulSkyRenderer)
		simulSkyRenderer->Destroy();
	simulSkyRenderer=NULL;

	if(simulCloudRenderer)
		simulCloudRenderer->Destroy();
	
	simulCloudRenderer=NULL;

	simulAtmosphericsRenderer=NULL;
	return (hr==S_OK);
}

SimulWeatherRendererDX11::~SimulWeatherRendererDX11()
{
	InvalidateDeviceObjects();
	// Free this memory here instead of global destruction(as causes shutdown problems).
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	Destroy();
	del(simulSkyRenderer,memoryInterface);
	del(simulCloudRenderer,memoryInterface);
	del(simulPrecipitationRenderer,memoryInterface);
	del(simulAtmosphericsRenderer,memoryInterface);
	del(simul2DCloudRenderer,memoryInterface);
	del(simulLightningRenderer,memoryInterface);
}

void SimulWeatherRendererDX11::SaveCubemapToFile(const char *filename_utf8,float exposure,float gamma)
{
	std::wstring filenamew=simul::base::Utf8ToWString(filename_utf8);
	ID3D11DeviceContext* m_pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	CubemapFramebuffer	fb_cubemap;
	static int cubesize=1024;
	fb_cubemap.SetWidthAndHeight(cubesize,cubesize);
	fb_cubemap.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	fb_cubemap.RestoreDeviceObjects(m_pd3dDevice);
	simul::dx11::Framebuffer gamma_correct;
	gamma_correct.SetWidthAndHeight(cubesize,cubesize);
	gamma_correct.RestoreDeviceObjects(m_pd3dDevice);

	ID3D1xEffectTechnique *tech=m_pTonemapEffect->GetTechniqueByName("simul_gamma");

	cam_pos=GetCameraPosVector(view);
	D3DXMATRIX view_matrices[6];
	MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
	bool noise3d=environment->cloudKeyframer->GetUse3DNoise();
	bool godrays=GetBaseAtmosphericsRenderer()->GetShowGodrays();
	GetBaseAtmosphericsRenderer()->SetShowGodrays(false);
	environment->cloudKeyframer->SetUse3DNoise(true);

	bool gamma_correction=false;//(filenamew.find(L".hdr")>=filenamew.length());
	int l=100;
	if(baseCloudRenderer)
	{
		baseCloudRenderer->RecompileShaders();
		l=baseCloudRenderer->GetMaxSlices(0);
		baseCloudRenderer->SetMaxSlices(0,250);
	}
	for(int i=0;i<6;i++)
	{
		fb_cubemap.SetCurrentFace(i);
		fb_cubemap.Activate(m_pImmediateContext);
		if(gamma_correction)
		{
			gamma_correct.Activate(m_pImmediateContext);
			gamma_correct.Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,0.f);
		}
		if(simulSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovRH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				600000.f);
			SetMatrices(view_matrices[i],cube_proj);
			RenderSkyAsOverlay(m_pImmediateContext,exposure,false,false,NULL,NULL,0,simul::sky::float4(0,0,1.f,1.f), true);
		}
		if(gamma_correction)
		{
			gamma_correct.Deactivate(m_pImmediateContext);
			simul::dx11::setParameter(m_pTonemapEffect,"imageTexture",gamma_correct.GetBufferResource());
			hdrConstants.gamma=gamma;
			hdrConstants.exposure=exposure;
			hdrConstants.Apply(m_pImmediateContext);
			ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
			gamma_correct.DrawQuad(m_pImmediateContext);
		}
		fb_cubemap.Deactivate(m_pImmediateContext);
	}
	ID3D11Texture2D *tex=fb_cubemap.GetCopy(m_pImmediateContext);
	//if(gamma_correction)
	{
		HRESULT hr=D3DX11SaveTextureToFileW(m_pImmediateContext,tex,D3DX11_IFF_DDS,filenamew.c_str());
	}
	//else
	{
		//vec4 target=new vec4(cubesize*cubesize*6);
		//fb_cubemap.CopyToMemory(target);
	}
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(tex);
	environment->cloudKeyframer->SetUse3DNoise(noise3d);
	if(baseCloudRenderer)
	{
		baseCloudRenderer->SetMaxSlices(0,l);
	}
	GetBaseAtmosphericsRenderer()->SetShowGodrays(godrays);
}

void SimulWeatherRendererDX11::RenderSkyAsOverlay(void *context,
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
	SIMUL_PROFILE_START("RenderSkyAsOverlay")
	SIMUL_GPU_PROFILE_START(context,"RenderSkyAsOverlay")
	BaseWeatherRenderer::RenderSkyAsOverlay(context,
											exposure,
											buffered,
											is_cubemap,
											mainDepthTexture,
											depthTextureForClouds,
											viewport_id,
											relativeViewportTextureRegionXYWH,
											doFinalCloudBufferToScreenComposite	);
	if(buffered&&doFinalCloudBufferToScreenComposite)
	{
		ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
		bool blend=!is_cubemap;
		imageTexture->SetResource(framebuffer.buffer_texture_SRV);
		simul::dx11::setParameter(m_pTonemapEffect,"depthTexture"		,(ID3D1xShaderResourceView*)mainDepthTexture);
		simul::dx11::setParameter(m_pTonemapEffect,"lowResDepthTexture"	,(ID3D1xShaderResourceView*)depthTextureForClouds);
		simul::dx11::setParameter(m_pTonemapEffect,"cloudDepthTexture"	,(ID3D1xShaderResourceView*)baseFramebuffer->GetDepthTex());
		ID3D1xEffectTechnique *tech=blend?SkyBlendTechnique:directTechnique;
		ApplyPass((ID3D11DeviceContext*)context,tech->GetPassByIndex(0));
		hdrConstants.exposure					=1.f;
		hdrConstants.gamma						=1.f;
		float max_fade_distance_metres			=baseSkyRenderer->GetSkyKeyframer()->GetMaxDistanceKm()*1000.f;
		hdrConstants.depthToLinFadeDistParams	=simul::math::Vector3(proj.m[3][2], max_fade_distance_metres, proj.m[2][2]*max_fade_distance_metres );

		hdrConstants.Apply(pContext);

		framebuffer.DrawQuad(pContext);
		imageTexture->SetResource(NULL);
		ApplyPass(pContext,tech->GetPassByIndex(0));
	}
	SIMUL_GPU_PROFILE_END(context,"RenderSkyAsOverlay")
	SIMUL_PROFILE_END
}

void SimulWeatherRendererDX11::RenderFramebufferDepth(void *context,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	//
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	HRESULT hr=S_OK;
	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	if(!environment->skyKeyframer)
		return;
	float max_fade_distance_metres=environment->skyKeyframer->GetMaxDistanceKm()*1000.f;
	UtilityRenderer::SetScreenSize(width,height);
	simul::dx11::setParameter(m_pTonemapEffect,"depthTexture"	,(ID3D1xShaderResourceView*)framebuffer.GetDepthTex());
	simul::dx11::setParameter(m_pTonemapEffect,"tanHalfFov"		,frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	simul::dx11::setParameter(m_pTonemapEffect,"nearZ"			,frustum.nearZ/max_fade_distance_metres);
	simul::dx11::setParameter(m_pTonemapEffect,"farZ"			,frustum.farZ/max_fade_distance_metres);
	simul::dx11::setParameter(m_pTonemapEffect,"depthToLinFadeDistParams", proj[14], max_fade_distance_metres, proj[10]*max_fade_distance_metres, 1.0f );
	int x=8;
	int y=height-w;
	UtilityRenderer::DrawQuad2(pContext,x,y,w,w,m_pTonemapEffect,m_pTonemapEffect->GetTechniqueByName("show_depth"));
}

void SimulWeatherRendererDX11::RenderLateCloudLayer(void *context,float exposure,bool,int viewport_id,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		const simul::sky::float4 viewportTextureRegionXYWH(0,0,1,1);
		simulCloudRenderer->Render(context,exposure,false,0,UseDefaultFog,true,viewport_id,viewportTextureRegionXYWH);
	}
}

void SimulWeatherRendererDX11::RenderPrecipitation(void *context)
{
	if(basePrecipitationRenderer)
		basePrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
	if(simulPrecipitationRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}

void SimulWeatherRendererDX11::RenderLightning(void *context,int viewport_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(context);
}

void SimulWeatherRendererDX11::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
	cam_pos=GetCameraPosVector(view);
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

SimulSkyRendererDX1x *SimulWeatherRendererDX11::GetSkyRenderer()
{
	return simulSkyRenderer;
}

SimulCloudRendererDX1x *SimulWeatherRendererDX11::GetCloudRenderer()
{
	return simulCloudRenderer;
}

Simul2DCloudRendererDX11 *SimulWeatherRendererDX11::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}
//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
void SimulWeatherRendererDX11::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
}

void *SimulWeatherRendererDX11::GetCloudDepthTexture()
{
	return framebuffer.GetDepthTex();
}
