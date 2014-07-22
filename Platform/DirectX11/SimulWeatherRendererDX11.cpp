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
	,numOldViewports(0)
	,m_pOldDepthSurface(NULL)
{
	m_pOldRenderTargets[0]=m_pOldRenderTargets[1]=NULL;
}

void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	crossplatform::TwoResFramebuffer::RestoreDeviceObjects(r);
	if(!r)
		return;
	m_pd3dDevice=(ID3D11Device*	)r->AsD3D11Device();
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	lowResFarFramebufferDx11	.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	lowResNearFramebufferDx11	.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	hiResFarFramebufferDx11		.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	hiResNearFramebufferDx11	.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);

	lowResFarFramebufferDx11	.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	lowResNearFramebufferDx11	.SetDepthFormat(0);
	hiResFarFramebufferDx11		.SetDepthFormat(0);
	hiResNearFramebufferDx11	.SetDepthFormat(0);

	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth				=(Width+Downscale-1)/Downscale;
	int BufferHeight			=(Height+Downscale-1)/Downscale;
	int W						=(Width+ds2-1)/ds2;
	int H						=(Height+ds2-1)/ds2;
	lowResFarFramebufferDx11	.SetWidthAndHeight(BufferWidth,BufferHeight);
	lowResNearFramebufferDx11	.SetWidthAndHeight(BufferWidth,BufferHeight);
	hiResFarFramebufferDx11		.SetWidthAndHeight(W,H);
	hiResNearFramebufferDx11	.SetWidthAndHeight(W,H);
	// We're going to TRY to encode near and far loss into two UINT's, for faster results
	lossTexture->ensureTexture2DSizeAndFormat(renderPlatform,W,H,crossplatform::RG_32_UINT,false,true);
	lowResFarFramebufferDx11	.RestoreDeviceObjects(r);
	lowResNearFramebufferDx11	.RestoreDeviceObjects(r);
	hiResFarFramebufferDx11		.RestoreDeviceObjects(r);
	hiResNearFramebufferDx11	.RestoreDeviceObjects(r);
}

void TwoResFramebuffer::SaveOldRTs(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->RSGetViewports(&numOldViewports,NULL);
	if(numOldViewports>0)
		pContext->RSGetViewports(&numOldViewports,m_OldViewports);
	m_pOldRenderTargets[0]=m_pOldRenderTargets[1]=NULL;
	m_pOldDepthSurface	=NULL;
	pContext->OMGetRenderTargets(	numOldViewports,
									m_pOldRenderTargets,
									&m_pOldDepthSurface
									);
	for(int i=numOldViewports;i<2;i++)
	{
		m_pOldRenderTargets[i]=NULL;
	}
}

void TwoResFramebuffer::ActivateHiRes(crossplatform::DeviceContext &deviceContext)
{
	SaveOldRTs(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	if(!GetHiResFarFramebuffer()->IsValid())
		GetHiResFarFramebuffer()->CreateBuffers();
	if(!GetHiResNearFramebuffer()->IsValid())
		GetHiResNearFramebuffer()->CreateBuffers();
	crossplatform::Texture * targs[]={GetHiResFarFramebuffer()->GetTexture(),GetHiResNearFramebuffer()->GetTexture()};
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,NULL);
	crossplatform::Viewport v[]={{0,0,Width/ds2,Height/ds2,0,1.f},{0,0,Width/ds2,Height/ds2,0,1.f}};
	renderPlatform->SetViewports(deviceContext,2,v);
}

void TwoResFramebuffer::DeactivateHiRes(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(	numOldViewports,
									m_pOldRenderTargets,
									m_pOldDepthSurface
									);
	SAFE_RELEASE(m_pOldRenderTargets[0]);
	SAFE_RELEASE(m_pOldRenderTargets[1]);
	SAFE_RELEASE(m_pOldDepthSurface);
	if(numOldViewports>0)
		pContext->RSSetViewports(numOldViewports,m_OldViewports);
}

void TwoResFramebuffer::ActivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	SaveOldRTs(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	if(!GetLowResFarFramebuffer()->IsValid())
		GetLowResFarFramebuffer()->CreateBuffers();
	if(!GetLowResNearFramebuffer()->IsValid())
		GetLowResNearFramebuffer()->CreateBuffers();
	crossplatform::Texture * targs[]={GetLowResFarFramebuffer()->GetTexture(),GetLowResNearFramebuffer()->GetTexture()};
	crossplatform::Texture * depth=GetLowResFarFramebuffer()->GetDepthTexture();
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,depth);
	crossplatform::Viewport v[]={{0,0,Width/Downscale,Height/Downscale,0,1.f},{0,0,Width/Downscale,Height/Downscale,0,1.f}};
	renderPlatform->SetViewports(deviceContext,2,v);
}

void TwoResFramebuffer::DeactivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->OMSetRenderTargets(	numOldViewports,
									m_pOldRenderTargets,
									m_pOldDepthSurface
									);
	SAFE_RELEASE(m_pOldRenderTargets[0]);
	SAFE_RELEASE(m_pOldRenderTargets[1]);
	SAFE_RELEASE(m_pOldDepthSurface);
	if(numOldViewports>0)
		pContext->RSSetViewports(numOldViewports,m_OldViewports);
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	lowResFarFramebufferDx11	.InvalidateDeviceObjects();
	lowResNearFramebufferDx11	.InvalidateDeviceObjects();
	hiResFarFramebufferDx11		.InvalidateDeviceObjects();
	hiResNearFramebufferDx11	.InvalidateDeviceObjects();
	crossplatform::TwoResFramebuffer::InvalidateDeviceObjects();
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
	sky::SkyKeyframer *sk			=env->skyKeyframer;
	clouds::CloudKeyframer *ck2d	=env->cloud2DKeyframer;
	clouds::CloudKeyframer *ck3d	=env->cloudKeyframer;
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

crossplatform::TwoResFramebuffer *SimulWeatherRendererDX11::GetFramebuffer(int view_id)
{
	if(framebuffers.find(view_id)==framebuffers.end())
	{
		dx11::TwoResFramebuffer *fb=new dx11::TwoResFramebuffer();
		framebuffers[view_id]=fb;
		fb->RestoreDeviceObjects(renderPlatform);
		return fb;
	}
	return framebuffers[view_id];
}

void SimulWeatherRendererDX11::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HRESULT hr=S_OK;
	renderPlatform=r;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
		i->second->RestoreDeviceObjects(renderPlatform);
	hdrConstants.RestoreDeviceObjects(renderPlatform);
	if(simulCloudRenderer)
	{
		simulCloudRenderer->RestoreDeviceObjects(renderPlatform);
		if(simulSkyRenderer)
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulLightningRenderer)
		simulLightningRenderer->RestoreDeviceObjects(renderPlatform);
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	}*/
	if(simulSkyRenderer)
	{
		simulSkyRenderer->RestoreDeviceObjects(renderPlatform);
	}

	if(simul2DCloudRenderer)
		simul2DCloudRenderer->RestoreDeviceObjects(renderPlatform);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->RestoreDeviceObjects(renderPlatform);

	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

void SimulWeatherRendererDX11::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_DELETE(effect);
	std::vector<crossplatform::EffectDefineOptions> opts;
	opts.push_back(crossplatform::CreateDefineOptions("REVERSE_DEPTH","0","1"));
	renderPlatform->EnsureEffectIsBuilt("simul_hdr",opts);
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	effect=renderPlatform->CreateEffect("simul_hdr",defines);
	hdrConstants.LinkToEffect(effect,"HdrConstants");
	BaseWeatherRenderer::RecompileShaders();
}

void SimulWeatherRendererDX11::InvalidateDeviceObjects()
{
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
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
	math::Matrix4x4 view_matrices[6];
	camera::MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
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
				simul::dx11::UtilityRenderer::DrawQuad(deviceContext);
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
	crossplatform::TwoResFramebuffer *fb=GetFramebuffer(deviceContext.viewStruct.view_id);
	if(buffered)
	{
		int w,h,s;
		fb->GetDimensions(w,h,s);
		if(hiResDepthTexture)
		{
			int S=s;
			if(lowResDepthTexture->width)
				S=hiResDepthTexture->width/lowResDepthTexture->width;
			if(S<1)
				S=1;
			fb->SetDimensions(hiResDepthTexture->width,hiResDepthTexture->length,s);
		}
		SIMUL_ASSERT(w!=0);
	}
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
		fb->GetLowResFarFramebuffer()->ActivateViewport(deviceContext,depthViewportXYWH.x,depthViewportXYWH.y,depthViewportXYWH.z,depthViewportXYWH.w);
		fb->GetLowResFarFramebuffer()->Clear(deviceContext.platform_context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.0f:1.0f);
	}

	crossplatform::MixedResolutionStruct mixedResolutionStruct(1,1,1);
	RenderLowResolutionElements(deviceContext,exposure,godrays_strength,is_cubemap,crossplatform::FAR_PASS,lowResDepthTexture,depthViewportXYWH,mixedResolutionStruct);

	if(buffered)
		fb->GetLowResFarFramebuffer()->Deactivate(deviceContext.platform_context);

	if(buffered&&doFinalCloudBufferToScreenComposite)
		CompositeCloudsToScreen(deviceContext,1.f,1.f,false,hiResDepthTexture,hiResDepthTexture,lowResDepthTexture,depthViewportXYWH,mixedResolutionStruct,mixedResolutionStruct);

	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
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
	return framebuffers[view_id]->GetLowResFarFramebuffer()->GetDepthTexture();
}

