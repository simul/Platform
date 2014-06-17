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
#if WINVER<0x602
#include <dxerr.h>
#endif
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Camera/Camera.h"
#include "SimulSkyRendererDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"
#include "PrecipitationRenderer.h"

#include "Simul/Platform/DirectX11/SaveTextureDX1x.h"
#include "SimulCloudRendererDX1x.h"
#include "Simul2DCloudRendererDX1x.h"
#include "LightningRenderer.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "MacrosDX1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

TwoResFramebuffer::TwoResFramebuffer()
	:m_pd3dDevice(NULL)
	,Width(0)
	,Height(0)
	,Downscale(0)
{
}

void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!r)
		return;
	m_pd3dDevice=(ID3D11Device*	)r->AsD3D11Device();
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
	
	lowResFarFramebufferDx11	.RestoreDeviceObjects(r);
	lowResNearFramebufferDx11	.RestoreDeviceObjects(r);
	hiResFarFramebufferDx11		.RestoreDeviceObjects(r);
	hiResNearFramebufferDx11	.RestoreDeviceObjects(r);
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
		RestoreDeviceObjects(renderPlatform);
	}
}

void TwoResFramebuffer::GetDimensions(int &w,int &h,int &downscale)
{
	w=Width;
	h=Height;
	downscale=Downscale;
}

SimulWeatherRendererDX11::SimulWeatherRendererDX11(simul::clouds::Environment *env
													,simul::base::MemoryInterface *mem) :
	BaseWeatherRenderer(env,mem),
	m_pd3dDevice(NULL)
	,simulSkyRenderer(NULL)
	,simulCloudRenderer(NULL)
	,simulPrecipitationRenderer(NULL)
	,simulAtmosphericsRenderer(NULL)
	,simul2DCloudRenderer(NULL)
	,simulLightningRenderer(NULL)
	,exposure_multiplier(1.f)
	,memoryInterface(mem)
{
	simul::sky::SkyKeyframer *sk		=env->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d	=env->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d	=env->cloudKeyframer;
	simulSkyRenderer			=::new(memoryInterface) SimulSkyRendererDX1x(sk, memoryInterface);
	baseSkyRenderer				=simulSkyRenderer;
	
	simulCloudRenderer			=::new(memoryInterface) SimulCloudRendererDX1x(ck3d, memoryInterface);
	baseCloudRenderer			=simulCloudRenderer;
	baseLightningRenderer		=simulLightningRenderer	=::new(memoryInterface) LightningRenderer(ck3d,sk);
	if(env->cloud2DKeyframer)
		base2DCloudRenderer		=simul2DCloudRenderer		=::new(memoryInterface) Simul2DCloudRendererDX11(ck2d, memoryInterface);
	basePrecipitationRenderer	=simulPrecipitationRenderer	=::new(memoryInterface) PrecipitationRenderer();
	baseAtmosphericsRenderer	=simulAtmosphericsRenderer	=::new(memoryInterface) SimulAtmosphericsRendererDX1x(mem);

	ConnectInterfaces();
}

TwoResFramebuffer *SimulWeatherRendererDX11::GetFramebuffer(int view_id)
{
	if(framebuffersDx11.find(view_id)==framebuffersDx11.end())
	{
		dx11::TwoResFramebuffer *fb=new dx11::TwoResFramebuffer();
		framebuffersDx11[view_id]=fb;
		fb->RestoreDeviceObjects(renderPlatform);
		return fb;
	}
	return framebuffersDx11[view_id];
}

void SimulWeatherRendererDX11::SetScreenSize(int view_id,int w,int h)
{
	TwoResFramebuffer *fb=GetFramebuffer(view_id);
	fb->SetDimensions(w,h,Downscale);
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HRESULT hr=S_OK;
	renderPlatform=r;
	m_pd3dDevice=(ID3D11Device*)renderPlatform->GetDevice();
	for(FramebufferMapDx11::iterator i=framebuffersDx11.begin();i!=framebuffersDx11.end();i++)
		i->second->RestoreDeviceObjects(renderPlatform);
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	if(simulCloudRenderer)
	{
		simulCloudRenderer->RestoreDeviceObjects(renderPlatform);
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
		simulSkyRenderer->RestoreDeviceObjects(renderPlatform);
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	//V_RETURN(CreateBuffers());

	if(simul2DCloudRenderer)
		simul2DCloudRenderer->RestoreDeviceObjects(renderPlatform);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->RestoreDeviceObjects(renderPlatform);

	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void SimulWeatherRendererDX11::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	delete effect;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	effect=new dx11::Effect(renderPlatform,"simul_hdr.fx",defines);
	hdrConstants.LinkToEffect(effect,"HdrConstants");
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	for(FramebufferMapDx11::iterator i=framebuffersDx11.begin();i!=framebuffersDx11.end();i++)
		i->second->InvalidateDeviceObjects();
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
	BaseWeatherRenderer::InvalidateDeviceObjects();
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

void SimulWeatherRendererDX11::SaveCubemapToFile(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,float exposure,float gamma)
{
	std::wstring filenamew=simul::base::Utf8ToWString(filename_utf8);
	ID3D11DeviceContext* m_pImmediateContext=NULL;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pImmediateContext;
	CubemapFramebuffer	fb_cubemap;
	static int cubesize=1024;
	fb_cubemap.SetWidthAndHeight(cubesize,cubesize);
	fb_cubemap.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	fb_cubemap.RestoreDeviceObjects(renderPlatform);
	dx11::Framebuffer gamma_correct;
	gamma_correct.SetWidthAndHeight(cubesize,cubesize);
	gamma_correct.RestoreDeviceObjects(renderPlatform);

	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("exposure_gamma");
	math::Matrix4x4 view;
	view.Identity();
	math::Vector3 cam_pos=GetCameraPosVector(view);
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
		fb_cubemap.Activate(deviceContext);
		if(gamma_correction)
		{
			gamma_correct.Activate(deviceContext);
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
	
			crossplatform::DeviceContext deviceContext;
			deviceContext.platform_context	=m_pImmediateContext;
			deviceContext.renderPlatform	=renderPlatform;
			deviceContext.viewStruct.view_id=0;
			deviceContext.viewStruct.proj	=(const float*)&cube_proj;
			deviceContext.viewStruct.view	=(const float*)&view_matrices[i];
			SetMatrices((const float*)&view_matrices[i],(const float*)&cube_proj);
			RenderSkyAsOverlay(deviceContext
				,false,exposure,false,NULL,NULL,simul::sky::float4(0,0,1.f,1.f),true);
			if(gamma_correction)
			{
				gamma_correct.Deactivate(m_pImmediateContext);
				simul::dx11::setTexture((ID3DX11Effect*)effect->platform_effect,"imageTexture",gamma_correct.GetBufferResource());
				hdrConstants.gamma=gamma;
				hdrConstants.exposure=exposure;
				hdrConstants.Apply(deviceContext);
				deviceContext.renderPlatform->ApplyShaderPass(deviceContext,effect,tech,0);
				//ApplyPass(m_pImmediateContext,((ID3DX11EffectTechnique*)tech->platform_technique)->GetPassByIndex(0));
				simul::dx11::UtilityRenderer::DrawQuad(m_pImmediateContext);
			}
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

void SimulWeatherRendererDX11::RenderSkyAsOverlay(crossplatform::DeviceContext &deviceContext
											,bool is_cubemap
											,float exposure
											,bool buffered
											,crossplatform::Texture *hiResDepthTexture			// x=far, y=near, z=edge
											,crossplatform::Texture* lowResDepthTexture			// x=far, y=near, z=edge
											,const sky::float4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite
											)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"RenderSkyAsOverlay")
	TwoResFramebuffer *fb=GetFramebuffer(deviceContext.viewStruct.view_id);
ERRNO_CHECK
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderAsOverlay(deviceContext,hiResDepthTexture,exposure,depthViewportXYWH);
ERRNO_CHECK
	//if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
	//	base2DCloudRenderer->Render(context,exposure,false,false,mainDepthTexture,false,view_id,depthViewportXYWH);
	// Now we render the low-resolution elements to the low-res buffer.
	float godrays_strength		=(float)(!is_cubemap)*environment->cloudKeyframer->GetInterpolatedKeyframe().godray_strength;
	if(buffered)
	{
		fb->lowResFarFramebufferDx11.ActivateViewport(deviceContext,depthViewportXYWH.x,depthViewportXYWH.y,depthViewportXYWH.z,depthViewportXYWH.w);
		fb->lowResFarFramebufferDx11.Clear(deviceContext.platform_context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	}
ERRNO_CHECK
	crossplatform::MixedResolutionStruct mixedResolutionStruct(1,1,1);
	RenderLowResolutionElements(deviceContext,exposure,godrays_strength,is_cubemap,false,lowResDepthTexture,depthViewportXYWH,mixedResolutionStruct);
ERRNO_CHECK
	if(buffered)
		fb->lowResFarFramebufferDx11.Deactivate(deviceContext.platform_context);
ERRNO_CHECK
	if(buffered&&doFinalCloudBufferToScreenComposite)
		CompositeCloudsToScreen(deviceContext,1.f,1.f,false,hiResDepthTexture,hiResDepthTexture,lowResDepthTexture,depthViewportXYWH,mixedResolutionStruct);
ERRNO_CHECK
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void SimulWeatherRendererDX11::CompositeCloudsToScreen(crossplatform::DeviceContext &deviceContext
												,float exposure
												,float gamma
												,bool depth_blend
												,crossplatform::Texture *mainDepthTexture
												,crossplatform::Texture* hiResDepthTexture
												,crossplatform::Texture* lowResDepthTexture
												,const simul::sky::float4& depthViewportXYWH
												,const crossplatform::MixedResolutionStruct &mixedResolutionStruct)
{
	TwoResFramebuffer *fb=GetFramebuffer(deviceContext.viewStruct.view_id);
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
	ID3DX11Effect *e=(ID3DX11Effect*)effect->platform_effect;
	dx11::setTexture(e,"imageTexture",fb->lowResFarFramebufferDx11.buffer_texture.shaderResourceView);
	bool msaa=false;
	if(mainDepthTexture)
	{
		ID3D11ShaderResourceView *mainDepth_SRV=mainDepthTexture->AsD3D11ShaderResourceView();
		if(mainDepth_SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			mainDepth_SRV->GetDesc(&desc);
			msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
		}
		if(msaa)
		{
			// Set both regular and MSAA depth variables. Which it is depends on the situation.
			dx11::setTexture(e,"depthTextureMS"		,mainDepth_SRV);
		}
		else
			dx11::setTexture(e,"depthTexture"			,mainDepth_SRV);
	}
	// The low res depth texture contains the total near and far depths in its x and y.
	effect->SetTexture("hiResDepthTexture"		,hiResDepthTexture);
	effect->SetTexture("lowResDepthTexture"		,lowResDepthTexture);
	effect->SetTexture("nearImageTexture"		,&fb->lowResNearFramebufferDx11.buffer_texture);
	effect->SetTexture("inscatterTexture"		,fb->hiResFarFramebufferDx11.GetTexture());
	effect->SetTexture("nearInscatterTexture"	,fb->hiResNearFramebufferDx11.GetTexture());

	crossplatform::EffectTechnique *t			=depth_blend?farNearDepthBlendTechnique:simpleCloudBlendTechnique;
	ID3DX11EffectTechnique *tech				=(ID3DX11EffectTechnique *)t->platform_technique;
	ApplyPass(pContext,tech->GetPassByIndex(msaa?1:0));
	hdrConstants.exposure						=exposure;
	hdrConstants.gamma							=gamma;
	hdrConstants.viewportToTexRegionScaleBias	=vec4(depthViewportXYWH.z, depthViewportXYWH.w, depthViewportXYWH.x, depthViewportXYWH.y);
	float max_fade_distance_metres				=baseSkyRenderer->GetSkyKeyframer()->GetMaxDistanceKm()*1000.f;
	hdrConstants.depthToLinFadeDistParams		=simul::math::Vector3(deviceContext.viewStruct.proj.m[3][2], max_fade_distance_metres, deviceContext.viewStruct.proj.m[2][2]*max_fade_distance_metres );
	hdrConstants.hiResToLowResTransformXYWH		=mixedResolutionStruct.GetTransformHiResToLowRes();
	hdrConstants.Apply(deviceContext);
	deviceContext.renderPlatform->DrawQuad(deviceContext);
	dx11::setTexture(e,"imageTexture",NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
}

void SimulWeatherRendererDX11::RenderFramebufferDepth(crossplatform::DeviceContext &deviceContext,int x,int y,int w,int h)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	TwoResFramebuffer *fb=GetFramebuffer(deviceContext.viewStruct.view_id);
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);

	if(!environment->skyKeyframer)
		return;
	float max_fade_distance_metres		=environment->skyKeyframer->GetMaxDistanceKm()*1000.f;
	effect->SetTexture("depthTexture"	,fb->lowResFarFramebufferDx11.GetDepthTexture());
	ID3DX11Effect *d3deffect			=(ID3DX11Effect*)effect->platform_effect;
	dx11::setParameter(d3deffect,"tanHalfFov"					,vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov));
	dx11::setParameter(d3deffect,"nearZ"						,frustum.nearZ/max_fade_distance_metres);
	dx11::setParameter(d3deffect,"farZ"							,frustum.farZ/max_fade_distance_metres);
	dx11::setParameter(d3deffect,"depthToLinFadeDistParams"		,vec3(deviceContext.viewStruct.proj[14], max_fade_distance_metres, deviceContext.viewStruct.proj[10]*max_fade_distance_metres));
	deviceContext.renderPlatform->DrawQuad(deviceContext	,x,y,w,h	,effect,effect->GetTechniqueByName("show_depth"));
}


void SimulWeatherRendererDX11::RenderPrecipitation(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex
												   ,simul::sky::float4 depthViewportXYWH)
{
	math::Vector3 cam_pos=GetCameraPosVector(deviceContext.viewStruct.view);
	if(basePrecipitationRenderer)
		basePrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
	float max_fade_dist_metres=1.f;
	if(environment->skyKeyframer)
		max_fade_dist_metres=environment->skyKeyframer->GetMaxDistanceKm()*1000.f;
	if(simulPrecipitationRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(deviceContext,depth_tex,max_fade_dist_metres,depthViewportXYWH);
}

void SimulWeatherRendererDX11::RenderLightning(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depth_tex,simul::sky::float4 depthViewportXYWH,crossplatform::Texture *low_res_depth_tex)
{
	if(simulCloudRenderer&&simulLightningRenderer&&baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(deviceContext,depth_tex,depthViewportXYWH,low_res_depth_tex);
}

void SimulWeatherRendererDX11::SetMatrices(const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p)
{
	//view=v;
	//proj=p;
	//cam_pos=GetCameraPosVector(view);
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(v,p);
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

crossplatform::Texture *SimulWeatherRendererDX11::GetCloudDepthTexture(int view_id)
{
	return framebuffersDx11[view_id]->GetLowResFarFramebuffer()->GetDepthTexture();
}

