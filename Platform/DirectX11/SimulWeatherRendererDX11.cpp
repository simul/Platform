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
#include "PrecipitationRenderer.h"

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
	nearFramebuffer(0,0),
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
		base2DCloudRenderer		=simul2DCloudRenderer		=::new(memoryInterface) Simul2DCloudRendererDX11(ck2d, memoryInterface);
	basePrecipitationRenderer	=simulPrecipitationRenderer=::new(memoryInterface) PrecipitationRenderer();
	baseAtmosphericsRenderer	=simulAtmosphericsRenderer	=::new(memoryInterface) SimulAtmosphericsRendererDX1x(mem);

	ConnectInterfaces();
}

void SimulWeatherRendererDX11::SetScreenSize(int view_id,int w,int h)
{
	ScreenWidth		=w;
	ScreenHeight	=h;
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	BufferWidth		=(ScreenWidth+Downscale-1)/Downscale;
	BufferHeight	=(ScreenHeight+Downscale-1)/Downscale;
	BufferSizeChanged(view_id);
}

void SimulWeatherRendererDX11::BufferSizeChanged(int view_id)
{
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	BufferWidth		=(ScreenWidth+Downscale-1)/Downscale;
	BufferHeight	=(ScreenHeight+Downscale-1)/Downscale;
	
	if(framebuffers.find(view_id)==framebuffers.end())
	{
		Framebuffer *f=new Framebuffer(ScreenWidth,ScreenHeight);
		f->SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
		f->RestoreDeviceObjects(m_pd3dDevice);
		framebuffers[view_id]=f;
		
		Framebuffer *n=new Framebuffer(ScreenWidth,ScreenHeight);
		n->SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
		n->RestoreDeviceObjects(m_pd3dDevice);
		nearFramebuffers[view_id]=f;

		f=new Framebuffer(BufferWidth,BufferHeight);
		f->SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
		f->RestoreDeviceObjects(m_pd3dDevice);
		lowResFramebuffers[view_id]=f;
		
		n=new Framebuffer(BufferWidth,BufferHeight);
		n->SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
		n->RestoreDeviceObjects(m_pd3dDevice);
		lowResNearFramebuffers[view_id]=f;
	}
	else
	{
		framebuffers[view_id]			->SetWidthAndHeight(ScreenWidth,ScreenHeight);
		nearFramebuffers[view_id]		->SetWidthAndHeight(ScreenWidth,ScreenHeight);
		lowResFramebuffers[view_id]		->SetWidthAndHeight(BufferWidth,BufferHeight);
		lowResNearFramebuffers[view_id]	->SetWidthAndHeight(BufferWidth,BufferHeight);
	}
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D1xDevice*)dev;
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
		i->second->RestoreDeviceObjects(m_pd3dDevice);
	for(FramebufferMap::iterator i=nearFramebuffers.begin();i!=nearFramebuffers.end();i++)
		i->second->RestoreDeviceObjects(m_pd3dDevice);
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
	directTechnique				=m_pTonemapEffect->GetTechniqueByName("simul_direct");
	showDepthTechnique			=m_pTonemapEffect->GetTechniqueByName("show_depth");
	farNearDepthBlendTechnique	=m_pTonemapEffect->GetTechniqueByName("far_near_depth_blend");
	imageTexture				=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	hdrConstants.LinkToEffect(m_pTonemapEffect,"HdrConstants");
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pTonemapEffect);
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
		i->second->InvalidateDeviceObjects();
	for(FramebufferMap::iterator i=nearFramebuffers.begin();i!=nearFramebuffers.end();i++)
		i->second->InvalidateDeviceObjects();
	hdrConstants.InvalidateDeviceObjects();
	nearFramebuffer.InvalidateDeviceObjects();
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

	ID3D1xEffectTechnique *tech=m_pTonemapEffect->GetTechniqueByName("exposure_gamma");

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
			SetMatrices((const float*)&view_matrices[i],(const float*)&cube_proj);
			RenderSkyAsOverlay(m_pImmediateContext,exposure,false,false,NULL,NULL,0,simul::sky::float4(0,0,1.f,1.f), true);
		}
		if(gamma_correction)
		{
			gamma_correct.Deactivate(m_pImmediateContext);
			simul::dx11::setTexture(m_pTonemapEffect,"imageTexture",gamma_correct.GetBufferResource());
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
												const void* lowResDepthTexture,
												int view_id,
												const simul::sky::float4& viewportRegionXYWH,
												bool doFinalCloudBufferToScreenComposite
												)
{
	SIMUL_PROFILE_START("RenderSkyAsOverlay")
	SIMUL_GPU_PROFILE_START(context,"RenderSkyAsOverlay")
/*	BaseWeatherRenderer::RenderSkyAsOverlay(context,
											exposure,
											buffered,
											is_cubemap,
											mainDepthTexture,
											lowResDepthTexture,
											view_id,
											viewportRegionXYWH,
											doFinalCloudBufferToScreenComposite	);*/

	RenderFullResolutionElements(context,exposure,mainDepthTexture,view_id,viewportRegionXYWH);
	float godrays_strength		=(float)(!is_cubemap)*environment->cloudKeyframer->GetInterpolatedKeyframe().godray_strength;
	BaseFramebuffer *fb			=NULL;
	if(buffered&&framebuffers.find(view_id)!=framebuffers.end())
		fb=framebuffers[view_id];
	if(buffered&&fb)
	{
		// No need for cloud depth texture if godrays are not active.
	/*	if(godrays_strength==0)
		{
			baseFramebuffer->ActivateColour(context,relativeViewportTextureRegionXYWH);
			baseFramebuffer->ClearColour(context,0.0f,0.0f,0.f,1.f);
	}
		else*/
		{
			fb->ActivateViewport(context,viewportRegionXYWH.x,viewportRegionXYWH.y,viewportRegionXYWH.z,viewportRegionXYWH.w);
			fb->Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
		}
	}
	RenderLowResolutionElements(context,exposure,godrays_strength,is_cubemap,false,lowResDepthTexture,view_id,viewportRegionXYWH);
	if(buffered)
	{
		fb->Deactivate(context);
		BaseFramebuffer *near_fb			=NULL;
		if(buffered&&nearFramebuffers.find(view_id)!=nearFramebuffers.end())
			near_fb							=nearFramebuffers[view_id];
		if(buffered&&near_fb)
		{
			near_fb->ActivateColour(context,viewportRegionXYWH);
			near_fb->ClearColour(context,0.0f,0.0f,0.f,1.f);
			RenderLowResolutionElements(context,exposure,godrays_strength,is_cubemap,true,lowResDepthTexture,view_id,viewportRegionXYWH);
			near_fb->Deactivate(context);
		}
		if(doFinalCloudBufferToScreenComposite)
		{
			ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
			bool blend=!is_cubemap;
			imageTexture->SetResource((ID3D1xShaderResourceView*)fb->GetColorTex());
			// Set both regular and MSAA depth variables. Which it is depends on the situation.
			simul::dx11::setTexture(m_pTonemapEffect,"depthTexture"			,(ID3D1xShaderResourceView*)mainDepthTexture);
			simul::dx11::setTexture(m_pTonemapEffect,"depthTextureMS"		,(ID3D1xShaderResourceView*)mainDepthTexture);
			// The low res depth texture contains the total near and far depths in its x and y.
			simul::dx11::setTexture(m_pTonemapEffect,"lowResDepthTexture"	,(ID3D1xShaderResourceView*)lowResDepthTexture);
			simul::dx11::setTexture(m_pTonemapEffect,"cloudDepthTexture"	,(ID3D1xShaderResourceView*)fb->GetDepthTex());
			simul::dx11::setTexture(m_pTonemapEffect,"nearImageTexture"		,(ID3D1xShaderResourceView*)near_fb->GetColorTex());
			ID3D1xEffectTechnique *tech=blend?farNearDepthBlendTechnique:directTechnique;
			ApplyPass((ID3D11DeviceContext*)context,tech->GetPassByIndex(0));
			hdrConstants.exposure					=1.f;
			hdrConstants.gamma						=1.f;
			hdrConstants.viewportToTexRegionScaleBias=vec4(viewportRegionXYWH.z, viewportRegionXYWH.w, viewportRegionXYWH.x, viewportRegionXYWH.y);
			float max_fade_distance_metres			=baseSkyRenderer->GetSkyKeyframer()->GetMaxDistanceKm()*1000.f;
			hdrConstants.depthToLinFadeDistParams	=simul::math::Vector3(proj.m[3][2], max_fade_distance_metres, proj.m[2][2]*max_fade_distance_metres );
			hdrConstants.lowResTexelSize			=vec2(1.0f/(float)BufferWidth,1.0f/(float)BufferHeight);
			hdrConstants.Apply(pContext);
			UtilityRenderer::DrawQuad(pContext);
			imageTexture->SetResource(NULL);
			ApplyPass(pContext,tech->GetPassByIndex(0));
		}
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
	BaseFramebuffer *fb=framebuffers.begin()->second;
	simul::dx11::setTexture(m_pTonemapEffect,"depthTexture"	,(ID3D1xShaderResourceView*)fb->GetDepthTex());
	int x=8;
	int y=height-w;

	hdrConstants.tanHalfFov					=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	hdrConstants.nearZ						=frustum.nearZ/max_fade_distance_metres;
	hdrConstants.farZ						=frustum.farZ/max_fade_distance_metres;
	hdrConstants.depthToLinFadeDistParams	=vec3(proj[14], max_fade_distance_metres, proj[10]*max_fade_distance_metres);
	hdrConstants.rect						=vec4((float)x/(float)width
												,(float)y/(float)height
												,(float)w/(float)width
												,(float)w/(float)height);
	hdrConstants.Apply(pContext);
	UtilityRenderer::DrawQuad2(pContext,x,y,w,w,m_pTonemapEffect,m_pTonemapEffect->GetTechniqueByName("show_depth"));
}

void SimulWeatherRendererDX11::RenderPrecipitation(void *context)
{
	if(basePrecipitationRenderer)
		basePrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
	if(simulPrecipitationRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}

void SimulWeatherRendererDX11::RenderLightning(void *context,int view_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(context);
}

void SimulWeatherRendererDX11::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
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
	return framebuffers.begin()->second->GetDepthTex();
}
