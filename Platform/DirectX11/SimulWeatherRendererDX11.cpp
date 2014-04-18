#define NOMINMAX
// Copyright (c) 2007-2014 Simul Software Ltd
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
#include "LightningRenderer.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
extern simul::dx11::RenderPlatform renderPlatformDx11;

using namespace simul;
using namespace dx11;

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
	m_pd3dDevice=(ID3D11Device*	)dev;
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	lowResFarFramebufferDx11	.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
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
	lowResFarFramebufferDx11	.InvalidateDeviceObjects();
	lowResNearFramebufferDx11	.InvalidateDeviceObjects();
	hiResFarFramebufferDx11		.InvalidateDeviceObjects();
	hiResNearFramebufferDx11	.InvalidateDeviceObjects();
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

SimulWeatherRendererDX11::SimulWeatherRendererDX11(simul::clouds::Environment *env
													,simul::base::MemoryInterface *mem) :
	BaseWeatherRenderer(env,mem),
	m_pd3dDevice(NULL),
	m_pTonemapEffect(NULL)
	,simpleCloudBlendTechnique(NULL)
	,imageTexture(NULL)
	,simulSkyRenderer(NULL)
	,simulCloudRenderer(NULL)
	,simulPrecipitationRenderer(NULL)
	,simulAtmosphericsRenderer(NULL)
	,simul2DCloudRenderer(NULL)
	,simulLightningRenderer(NULL)
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
	baseLightningRenderer	=simulLightningRenderer	=::new(memoryInterface) LightningRenderer(ck3d,sk);
	if(env->cloud2DKeyframer)
		base2DCloudRenderer		=simul2DCloudRenderer		=::new(memoryInterface) Simul2DCloudRendererDX11(ck2d, memoryInterface);
	basePrecipitationRenderer	=simulPrecipitationRenderer=::new(memoryInterface) PrecipitationRenderer();
	baseAtmosphericsRenderer	=simulAtmosphericsRenderer	=::new(memoryInterface) SimulAtmosphericsRendererDX1x(mem);

	ConnectInterfaces();
}

TwoResFramebuffer *SimulWeatherRendererDX11::GetFramebuffer(int view_id)
{
	if(framebuffersDx11.find(view_id)==framebuffersDx11.end())
	{
		dx11::TwoResFramebuffer *fb=new dx11::TwoResFramebuffer();
		framebuffersDx11[view_id]=fb;
		fb->RestoreDeviceObjects(m_pd3dDevice);
		return fb;
	}
	return framebuffersDx11[view_id];
}

void SimulWeatherRendererDX11::SetScreenSize(int view_id,int w,int h)
{
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	fb->SetDimensions(w,h,Downscale);
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(void* dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=(ID3D11Device*)dev;
	for(FramebufferMapDx11::iterator i=framebuffersDx11.begin();i!=framebuffersDx11.end();i++)
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
	simpleCloudBlendTechnique	=m_pTonemapEffect->GetTechniqueByName("simple_cloud_blend");
	showDepthTechnique			=m_pTonemapEffect->GetTechniqueByName("show_depth");
	farNearDepthBlendTechnique	=m_pTonemapEffect->GetTechniqueByName("far_near_depth_blend");
	imageTexture				=m_pTonemapEffect->GetVariableByName("imageTexture")->AsShaderResource();
	hdrConstants.LinkToEffect(m_pTonemapEffect,"HdrConstants");
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pTonemapEffect);
	for(FramebufferMapDx11::iterator i=framebuffersDx11.begin();i!=framebuffersDx11.end();i++)
		i->second->InvalidateDeviceObjects();
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

	ID3DX11EffectTechnique *tech=m_pTonemapEffect->GetTechniqueByName("exposure_gamma");

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
			RenderSkyAsOverlay(m_pImmediateContext,0,(const float*)&view_matrices[i],(const float*)&cube_proj
				,false,exposure,false,NULL,NULL,simul::sky::float4(0,0,1.f,1.f),true);
		}
		if(gamma_correction)
		{
			gamma_correct.Deactivate(m_pImmediateContext);
			simul::dx11::setTexture(m_pTonemapEffect,"imageTexture",gamma_correct.GetBufferResource());
			hdrConstants.gamma=gamma;
			hdrConstants.exposure=exposure;
			hdrConstants.Apply(m_pImmediateContext);
			ApplyPass(m_pImmediateContext,tech->GetPassByIndex(0));
			simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
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

void SimulWeatherRendererDX11::RenderSkyAsOverlay(void *context
											,int view_id											
											,const math::Matrix4x4 &viewmat
											,const math::Matrix4x4 &projmat
											,bool is_cubemap
											,float exposure
											,bool buffered
											,const void* hiResDepthTexture			// x=far, y=near, z=edge
											,const void* lowResDepthTexture			// x=far, y=near, z=edge
											,const sky::float4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite
											)
{
	SIMUL_COMBINED_PROFILE_START(context,"RenderSkyAsOverlay")
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderAsOverlay(context,hiResDepthTexture,exposure,depthViewportXYWH);
	//if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
	//	base2DCloudRenderer->Render(context,exposure,false,false,mainDepthTexture,UseDefaultFog,false,view_id,depthViewportXYWH);
	// Now we render the low-resolution elements to the low-res buffer.
	float godrays_strength		=(float)(!is_cubemap)*environment->cloudKeyframer->GetInterpolatedKeyframe().godray_strength;
	if(buffered)
	{
		fb->lowResFarFramebufferDx11.ActivateViewport(context,depthViewportXYWH.x,depthViewportXYWH.y,depthViewportXYWH.z,depthViewportXYWH.w);
		fb->lowResFarFramebufferDx11.Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	}
	RenderLowResolutionElements(context,exposure,godrays_strength,is_cubemap,false,lowResDepthTexture,view_id,depthViewportXYWH);
	if(buffered)
		fb->lowResFarFramebufferDx11.Deactivate(context);
	if(buffered&&doFinalCloudBufferToScreenComposite)
		CompositeCloudsToScreen(context,view_id,false,hiResDepthTexture,lowResDepthTexture,depthViewportXYWH);
	SIMUL_COMBINED_PROFILE_END(context)
}

void SimulWeatherRendererDX11::RenderMixedResolution(	void *context
														,int view_id
														,const math::Matrix4x4 &viewmat
														,const math::Matrix4x4 &projmat
														,bool is_cubemap
														,float exposure
														,const void* mainDepthTextureMS	
														,const void* hiResDepthTexture	// x=far, y=near, z=edge
														,const void* lowResDepthTexture // x=far, y=near, z=edge
														,const sky::float4& depthViewportXYWH
														)
{
	SIMUL_COMBINED_PROFILE_START(context,"RenderMixedResolution")
#if 1
	SIMUL_GPU_PROFILE_START(context,"Loss")
	if(baseAtmosphericsRenderer)
		baseAtmosphericsRenderer->RenderLoss(context,hiResDepthTexture,depthViewportXYWH,false);
	
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	
	SIMUL_GPU_PROFILE_END(context)
	SIMUL_GPU_PROFILE_START(context,"Hi-Res FAR")
	fb->hiResFarFramebufferDx11.Activate(context);
	fb->hiResFarFramebufferDx11.Clear(context,0.0f,1.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	// RenderInscatter also clears the buffer.
	if(baseAtmosphericsRenderer)
		baseAtmosphericsRenderer->RenderInscatter(context,hiResDepthTexture,exposure,depthViewportXYWH,false);
	if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
		base2DCloudRenderer->Render(context,exposure,false,false,mainDepthTextureMS,UseDefaultFog,false,view_id,depthViewportXYWH);
	
	fb->hiResFarFramebufferDx11.Deactivate(context);
	
	SIMUL_GPU_PROFILE_END(context)
	SIMUL_GPU_PROFILE_START(context,"Hi-Res NEAR")
	fb->hiResNearFramebufferDx11.Activate(context);
	fb->hiResNearFramebufferDx11.Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderInscatter(context,hiResDepthTexture,exposure,depthViewportXYWH,true);
	
	fb->hiResNearFramebufferDx11.Deactivate(context);
	// Now do the near-pass
	float godrays_strength		=(float)(!is_cubemap)*environment->cloudKeyframer->GetInterpolatedKeyframe().godray_strength;
	
	SIMUL_GPU_PROFILE_END(context)
	SIMUL_GPU_PROFILE_START(context,"LO-RES FAR")
	// Now we will render the main, FAR pass, of the clouds.
	fb->lowResFarFramebufferDx11.ActivateViewport(context,depthViewportXYWH.x,depthViewportXYWH.y,depthViewportXYWH.z,depthViewportXYWH.w);
	fb->lowResFarFramebufferDx11.Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	if(basePrecipitationRenderer)
	{
		DepthTextureStruct depth={	lowResDepthTexture
									,depthViewportXYWH
									};
		crossplatform::ViewStruct viewStruct={	view_id
												,view
												,proj
												};
		//basePrecipitationRenderer->RenderMoisture(context
		//										,depth
		//										,viewStruct);
	}
	RenderLowResolutionElements(context,exposure,godrays_strength,is_cubemap,false,lowResDepthTexture,view_id,depthViewportXYWH);
	fb->lowResFarFramebufferDx11.Deactivate(context);
	
	SIMUL_GPU_PROFILE_END(context)
	SIMUL_GPU_PROFILE_START(context,"LO-RES NEAR")
	// Now we will render the secondary, NEAR pass of the clouds.
	fb->lowResNearFramebufferDx11.ActivateColour(context);
	fb->lowResNearFramebufferDx11.ClearColour(context,0.0f,0.0f,0.f,1.f);
	RenderLowResolutionElements(context,exposure,godrays_strength,is_cubemap,true,lowResDepthTexture,view_id,depthViewportXYWH);
	fb->lowResNearFramebufferDx11.Deactivate(context);
	SIMUL_GPU_PROFILE_END(context)
	SIMUL_GPU_PROFILE_START(context,"Composite")
	
	CompositeCloudsToScreen(context,view_id,!is_cubemap,mainDepthTextureMS,lowResDepthTexture,depthViewportXYWH);
	
	//RenderLightning(context,view_id,mainDepthTextureMS,depthViewportXYWH,GetCloudDepthTexture());
	RenderPrecipitation(context,lowResDepthTexture,depthViewportXYWH,view,proj);
	SIMUL_GPU_PROFILE_END(context)
#endif
	SIMUL_COMBINED_PROFILE_END(context)
}

void SimulWeatherRendererDX11::CompositeCloudsToScreen(void *context
												,int view_id
												,bool depth_blend
												,const void* mainDepthTexture
												,const void* lowResDepthTexture
												,const simul::sky::float4& depthViewportXYWH)
{
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	imageTexture->SetResource(fb->lowResFarFramebufferDx11.buffer_texture_SRV);
	ID3D11ShaderResourceView *depth_SRV=(ID3D11ShaderResourceView*)mainDepthTexture;
	bool msaa=false;
	if(depth_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		depth_SRV->GetDesc(&desc);
		msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	}
	if(msaa)
	{
		// Set both regular and MSAA depth variables. Which it is depends on the situation.
		//simul::dx11::setTexture(m_pTonemapEffect,"nearFarDepthTexture"			,(ID3D11ShaderResourceView*)nearFarDepthTexture);
		simul::dx11::setTexture(m_pTonemapEffect,"depthTextureMS"		,depth_SRV);
	}
//	else
	{
		simul::dx11::setTexture(m_pTonemapEffect,"depthTexture"			,depth_SRV);
	}
	// The low res depth texture contains the total near and far depths in its x and y.
	simul::dx11::setTexture(m_pTonemapEffect,"lowResDepthTexture"	,(ID3D11ShaderResourceView*)lowResDepthTexture);
	simul::dx11::setTexture(m_pTonemapEffect,"nearImageTexture"		,(ID3D11ShaderResourceView*)fb->lowResNearFramebufferDx11.buffer_texture_SRV);
	simul::dx11::setTexture(m_pTonemapEffect,"inscatterTexture"		,(ID3D11ShaderResourceView*)fb->hiResFarFramebufferDx11.GetColorTex());
	simul::dx11::setTexture(m_pTonemapEffect,"nearInscatterTexture"	,(ID3D11ShaderResourceView*)fb->hiResNearFramebufferDx11.GetColorTex());

	ID3DX11EffectTechnique *tech					=depth_blend?farNearDepthBlendTechnique:simpleCloudBlendTechnique;
	ApplyPass((ID3D11DeviceContext*)context,tech->GetPassByIndex(msaa?1:0));
	hdrConstants.exposure						=1.f;
	hdrConstants.gamma							=1.f;
	hdrConstants.viewportToTexRegionScaleBias	=vec4(depthViewportXYWH.z, depthViewportXYWH.w, depthViewportXYWH.x, depthViewportXYWH.y);
	float max_fade_distance_metres				=baseSkyRenderer->GetSkyKeyframer()->GetMaxDistanceKm()*1000.f;
	hdrConstants.depthToLinFadeDistParams		=simul::math::Vector3(proj.m[3][2], max_fade_distance_metres, proj.m[2][2]*max_fade_distance_metres );
	hdrConstants.Apply(pContext);
	simul::dx11::UtilityRenderer::DrawQuad(pContext);
	imageTexture->SetResource(NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
}

void SimulWeatherRendererDX11::RenderFramebufferDepth(void *context,int view_id,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	HRESULT hr=S_OK;
	static int u=4;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	if(!environment->skyKeyframer)
		return;
	float max_fade_distance_metres=environment->skyKeyframer->GetMaxDistanceKm()*1000.f;
	simul::dx11::setTexture(m_pTonemapEffect,"depthTexture"	,(ID3D11ShaderResourceView*)fb->lowResFarFramebufferDx11.GetDepthTex());
	int x=x0+8;
	int y=y0+height-(w+8);
	int screenWidth,screenHeight;
	UtilityRenderer::GetScreenSize(screenWidth,screenHeight);
	hdrConstants.tanHalfFov					=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	hdrConstants.nearZ						=frustum.nearZ/max_fade_distance_metres;
	hdrConstants.farZ						=frustum.farZ/max_fade_distance_metres;
	hdrConstants.depthToLinFadeDistParams	=vec3(proj[14], max_fade_distance_metres, proj[10]*max_fade_distance_metres);
	hdrConstants.rect						=vec4((float)x/(float)screenWidth
												,(float)y/(float)screenHeight
												,(float)w/(float)screenWidth
												,(float)w/(float)screenHeight);
	hdrConstants.Apply(pContext);
	UtilityRenderer::DrawQuad2(pContext,x,y,w,w,m_pTonemapEffect,m_pTonemapEffect->GetTechniqueByName("show_depth"));
}


void SimulWeatherRendererDX11::RenderPrecipitation(void *context,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
	if(basePrecipitationRenderer)
		basePrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
	float max_fade_dist_metres=1.f;
	if(environment->skyKeyframer)
		max_fade_dist_metres=environment->skyKeyframer->GetMaxDistanceKm()*1000.f;
	if(simulPrecipitationRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context,depth_tex,v,p,max_fade_dist_metres,depthViewportXYWH);
}

void SimulWeatherRendererDX11::RenderCompositingTextures(void *context,int view_id,int x0,int y0,int dx,int dy)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;

	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	int w=dx/2;
	int l=dy/2;
	renderPlatformDx11.DrawTexture(pContext	,x0+0*w	,y0+l	,w,l,(ID3D11ShaderResourceView*)fb->hiResFarFramebufferDx11.GetColorTex());
	UtilityRenderer::Print		(pContext	,x0+0*w	,y0+l	,"Hi-Res Far");
	renderPlatformDx11.DrawTexture(pContext	,x0+1*w	,y0+l	,w,l,(ID3D11ShaderResourceView*)fb->hiResNearFramebufferDx11.GetColorTex());
	UtilityRenderer::Print		(pContext	,x0+1*w	,y0+l	,"Hi-Res Near");
	renderPlatformDx11.DrawTexture(pContext	,x0+0*w	,y0+2*l	,w,l,(ID3D11ShaderResourceView*)fb->lowResFarFramebufferDx11.GetColorTex());
	UtilityRenderer::Print		(pContext	,x0+0*w	,y0+2*l	,"Lo-Res Far");
	renderPlatformDx11.DrawTexture(pContext	,x0+1*w	,y0+2*l	,w,l,(ID3D11ShaderResourceView*)fb->lowResNearFramebufferDx11.GetColorTex());
	UtilityRenderer::Print		(pContext	,x0+1*w	,y0+2*l	,"Lo-Res Near");
}

void SimulWeatherRendererDX11::RenderLightning(void *context,int view_id,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const void *low_res_depth_tex)
{
	if(simulCloudRenderer&&simulLightningRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(context,view,proj,depth_tex,depthViewportXYWH,low_res_depth_tex);
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
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(view,proj);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(view,proj);
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
	return framebuffersDx11.begin()->second->GetLowResFarFramebuffer()->GetDepthTex();
}
