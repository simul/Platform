#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/Crossplatform/Material.h"
#include "Simul/Platform/Crossplatform/DemoOverlay.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Terrain/BaseSeaRenderer.h"
#include "Simul/Platform/CrossPlatform/HDRRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/pi.h"
#ifdef SIMUL_USE_SCENE
#include "Simul/Scene/BaseSceneRenderer.h"
#endif
using namespace simul;
using namespace dx11;

TrueSkyRenderer::TrueSkyRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m)
		:clouds::TrueSkyRenderer(env,sc,m)
		,ShowWaterTextures(false)
		,ShowWater(true)
		,oceanRenderer(NULL)
{
	sc;
	simulWeatherRenderer	=::new(memoryInterface) clouds::BaseWeatherRenderer(env,memoryInterface);
	baseOpticsRenderer		=::new(memoryInterface) crossplatform::BaseOpticsRenderer(memoryInterface);
	ReverseDepthChanged();
}

TrueSkyRenderer::~TrueSkyRenderer()
{
	InvalidateDeviceObjects();
	del(baseOpticsRenderer,memoryInterface);
	del(simulWeatherRenderer,memoryInterface);
	del(baseTerrainRenderer,memoryInterface);
	del(oceanRenderer,memoryInterface);
}

void TrueSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	clouds::TrueSkyRenderer::RestoreDeviceObjects(r);
	renderPlatform=r;
	if(!renderPlatform)
		return;
	if(!oceanRenderer&&simulWeatherRenderer&&(simul::base::GetFeatureLevel()&simul::base::EXPERIMENTAL)!=0)
	{
		oceanRenderer			=new(memoryInterface) terrain::BaseSeaRenderer(simulWeatherRenderer->GetEnvironment()->seaKeyframer);
		oceanRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetEnvironment()->skyKeyframer);
	}
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}
// The elements in the main colour/depth buffer, with depth test and optional MSAA
void TrueSkyRenderer::RenderDepthElements(crossplatform::DeviceContext &deviceContext
									 ,float exposure
									 ,float gamma)
{
	if(baseTerrainRenderer&&ShowTerrain)
	{
		math::Vector3 cam_pos=simul::crossplatform::GetCameraPosVector(deviceContext.viewStruct.view);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			baseTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture(cam_pos));
		baseTerrainRenderer->Render(deviceContext,1.f);	
	}
#ifdef SIMUL_USE_SCENE
	if(sceneRenderer)
	{
		crossplatform::PhysicalLightRenderData physicalLightRenderData;
		physicalLightRenderData.diffuseCubemap=envmapFramebuffer->GetTexture();
			physicalLightRenderData.lightColour=simulWeatherRenderer->GetSkyKeyframer()->GetLocalSunIrradiance(simulWeatherRenderer->GetSkyKeyframer()->GetTime(),0.f);//GetLocalIrradiance(0.0f);
			physicalLightRenderData.dirToLight=simulWeatherRenderer->GetSkyKeyframer()->GetDirectionToLight(0.0f);
		sceneRenderer->Render(deviceContext,physicalLightRenderData);
	}
#endif
	if(oceanRenderer&&ShowWater&&(simul::base::GetFeatureLevel()&simul::base::EXPERIMENTAL)!=0)
	{
		oceanRenderer->SetMatrices(deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		oceanRenderer->Render(deviceContext,1.f);
		if(oceanRenderer->GetShowWireframes())
			oceanRenderer->RenderWireframe(deviceContext);
	}
}

void TrueSkyRenderer::RenderMixedResolutionSky(crossplatform::DeviceContext &deviceContext
									 ,crossplatform::Texture *depthTexture
									 ,float exposure
									 ,float gamma)
		{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"RenderMixedResolutionSky")
	crossplatform::MixedResolutionView *view=viewManager.GetView(deviceContext.viewStruct.view_id);
	// now we process that depth buffer:
	// Now we render using the data from these depth buffers into our offscreen sky/cloud buffers.
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetCubemapTexture(envmapFramebuffer->GetTexture());
		simul::sky::float4 depthViewportXYWH(0.0f,0.0f,1.0f,1.0f);
		simulWeatherRenderer->RenderMixedResolution(deviceContext
														,false
														,exposure
														,gamma
														,depthTexture
														,view->GetHiResDepthTexture()
														,trueSkyRenderMode==clouds::MIXED_RESOLUTION?view->GetLowResDepthTexture():NULL
														,depthViewportXYWH,view->pixelOffset);
		simulWeatherRenderer->CompositeCloudsToScreen(deviceContext
														,exposure
														,gamma
														,DepthBasedComposite
														,depthTexture
														,view->GetHiResDepthTexture()
														,trueSkyRenderMode==clouds::MIXED_RESOLUTION?view->GetLowResDepthTexture():NULL
														,depthViewportXYWH
														,view->pixelOffset);
		simulWeatherRenderer->RenderPrecipitation(deviceContext,view->GetHiResDepthTexture(),depthViewportXYWH);
		//if(simulHDRRenderer&&UseHdrPostprocessor)
			view->GetFramebuffer()->ActivateDepth(deviceContext);
		//else
		//	pContext->OMSetRenderTargets(1,&mainRenderTarget,mainDepthSurface);
		simulWeatherRenderer->RenderLightning(deviceContext,view->GetHiResDepthTexture(),depthViewportXYWH,simulWeatherRenderer->GetCloudDepthTexture(deviceContext.viewStruct.view_id));
		simulWeatherRenderer->DoOcclusionTests(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void TrueSkyRenderer::RenderToOculus(crossplatform::DeviceContext &deviceContext
				,const crossplatform::CameraViewStruct &cameraViewStruct)
{
	D3D11_VIEWPORT				viewport;
	memset(&viewport,0,sizeof(D3D11_VIEWPORT));
	viewport.MinDepth			=0.0f;
	viewport.MaxDepth			=1.0f;
	crossplatform::MixedResolutionView *view=viewManager.GetView(deviceContext.viewStruct.view_id);
	viewport.Width				=(float)view->GetScreenWidth()/2;
	viewport.Height				=(float)view->GetScreenHeight();
	bool hdr=simulHDRRenderer&&UseHdrPostprocessor;
	deviceContext.asD3D11DeviceContext()->RSSetViewports(1, &viewport);
	const crossplatform::CameraOutputInterface *cam		=cameras[deviceContext.viewStruct.view_id];
	//left eye
	if(cam)
	{
		deviceContext.viewStruct.proj=cam->MakeStereoProjectionMatrix(simul::crossplatform::LEFT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
		deviceContext.viewStruct.view=cam->MakeStereoViewMatrix(simul::crossplatform::LEFT_EYE);
	}
	view->GetFramebuffer()->Activate(deviceContext);
	view->GetFramebuffer()->Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, ReverseDepth ? 0.f : 1.f);
	
	{
		RenderDepthElements(deviceContext,cameraViewStruct.exposure,cameraViewStruct.gamma);
		view->GetFramebuffer()->DeactivateDepth(deviceContext);
	}
	if(simulWeatherRenderer)
	{
		crossplatform::Viewport viewport={0,0,view->GetFramebuffer()->GetDepthTexture()->width,view->GetFramebuffer()->GetDepthTexture()->length,0.f,1.f};
		viewManager.DownscaleDepth(deviceContext,view->GetFramebuffer()->GetDepthTexture(),NULL,simulWeatherRenderer->GetAtmosphericDownscale(),simulWeatherRenderer->GetDownscale()
			,simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetMaxDistanceKm()*1000.0f,true);
	}
	RenderMixedResolutionSky(deviceContext,view->GetFramebuffer()->GetDepthTexture(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	if(hdr)
	{
		view->GetFramebuffer()->Deactivate(deviceContext);
		simulHDRRenderer->RenderWithOculusCorrection(deviceContext,view->GetFramebuffer()->GetTexture(),cameraViewStruct.exposure,cameraViewStruct.gamma,0.f);
	}
	//right eye
	if(cam)
	{
		deviceContext.viewStruct.proj	=cam->MakeStereoProjectionMatrix(simul::crossplatform::RIGHT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
		deviceContext.viewStruct.view	=cam->MakeStereoViewMatrix(simul::crossplatform::RIGHT_EYE);
	}
	viewport.TopLeftX			=view->GetScreenWidth()/2.f;
	view->GetFramebuffer()->Activate(deviceContext);
	view->GetFramebuffer()->Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, ReverseDepth ? 0.f : 1.f);
	
	RenderDepthElements(deviceContext,cameraViewStruct.exposure,cameraViewStruct.gamma);
	view->GetFramebuffer()->DeactivateDepth(deviceContext);
	
	if(simulWeatherRenderer)
	{
		crossplatform::Viewport viewport={0,0,view->GetFramebuffer()->GetDepthTexture()->width,view->GetFramebuffer()->GetDepthTexture()->length,0.f,1.f};
		viewManager.DownscaleDepth(deviceContext,view->GetFramebuffer()->GetDepthTexture(),NULL,simulWeatherRenderer->GetAtmosphericDownscale(),simulWeatherRenderer->GetDownscale(),simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetMaxDistanceKm()*1000.0f,true);
	}
	RenderMixedResolutionSky(deviceContext,view->GetFramebuffer()->GetDepthTexture(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	if(hdr)
	{
		view->GetFramebuffer()->Deactivate(deviceContext);
		simulHDRRenderer->RenderWithOculusCorrection(deviceContext,view->GetFramebuffer()->GetTexture(),cameraViewStruct.exposure,cameraViewStruct.gamma,1.f);
	}
}

void TrueSkyRenderer::Render(int view_id,ID3D11DeviceContext* pContext)
{
	if(!renderPlatform)
		return;
	crossplatform::MixedResolutionView *view=viewManager.GetView(view_id);
	if(!view)
		return;
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=pContext;
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	/*		ID3D11RenderTargetView						*mainRenderTarget;
			ID3D11DepthStencilView						*mainDepthSurface;
	pContext->OMGetRenderTargets(	1,
									&mainRenderTarget,
									&mainDepthSurface
									);*/
	SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext.platform_context)
	crossplatform::Viewport 		viewport=renderPlatform->GetViewport(deviceContext,view_id);
	view->SetResolution((int)viewport.w,(int)viewport.h);
	EnsureCorrectBufferSizes(view_id);
	const crossplatform::CameraOutputInterface *cam		=cameras[view_id];
	SIMUL_ASSERT(cam!=NULL);
	const crossplatform::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	
	SetReverseDepth(cameraViewStruct.projection==crossplatform::DEPTH_REVERSE);
	if(cam)
	{
		float aspect=(float)view->GetScreenWidth()/(float)view->GetScreenHeight();
		if(ReverseDepth)
			deviceContext.viewStruct.proj=cam->MakeDepthReversedProjectionMatrix(aspect);
		else
			deviceContext.viewStruct.proj=cam->MakeProjectionMatrix(aspect);
		deviceContext.viewStruct.view=cam->MakeViewMatrix();
		deviceContext.viewStruct.depthTextureStyle=crossplatform::PROJECTION;
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->PreRenderUpdate(deviceContext);
	if(MakeCubemap)
	{
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Cubemap")
		const float *cam_pos=simul::crossplatform::GetCameraPosVector(deviceContext.viewStruct.view);
		RenderCubemap(deviceContext,cam_pos);
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Envmap")
		RenderEnvmap(deviceContext);
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	}
	if(view->viewType==crossplatform::OCULUS_VR)
		RenderToOculus(deviceContext,cameraViewStruct);
	else
	{
		RenderStandard(deviceContext,cameraViewStruct);
		RenderOverlays(deviceContext,cameraViewStruct);
	}
	SIMUL_COMBINED_PROFILE_ENDFRAME(pContext)
}

void TrueSkyRenderer::RenderStandard(crossplatform::DeviceContext &deviceContext,const crossplatform::CameraViewStruct &cameraViewStruct)
{
	ID3D11DeviceContext* pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Render")
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"1")
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Clear")
	crossplatform::MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
	bool hdr=simulHDRRenderer&&UseHdrPostprocessor;
	float exposure=hdr?1.f:cameraViewStruct.exposure,gamma=hdr?1.f:cameraViewStruct.gamma;

	view->GetFramebuffer()->Activate(deviceContext);
	view->GetFramebuffer()->Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, ReverseDepth ? 0.f : 1.f);
	
	// Here we render the depth elements. There are two possibilities:
	//		1. We render depth elements to the main view framebuffer, which is non-MSAA. Then we deactivate the depth buffer, and use it to continue rendering to the same buffer.
	//		2. We render depth elements to a MSAA framebuffer, then resolve this down to the main framebuffer. Then we use the MSAA depth buffer to continue rendering.
	if(Antialiasing>1)
	{
		msaaFramebuffer->SetAntialiasing(Antialiasing);
		msaaFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
		msaaFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);// NOTE: D16_UNORM does NOT work well here.
		msaaFramebuffer->SetWidthAndHeight(view->ScreenWidth,view->ScreenHeight);
		msaaFramebuffer->Activate(deviceContext);
		msaaFramebuffer->Clear(deviceContext, 0.f, 0.f, 0.f, 0.f, ReverseDepth ? 0.f : 1.f, 0);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
		// Don't need to pass a depth texture because we know depth is active here.
	if(simulWeatherRenderer)
		simulWeatherRenderer->RenderCelestialBackground(deviceContext, NULL,sky::float4(0,0,1.0f,1.0f), exposure);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"2")
	RenderDepthElements(deviceContext,exposure,gamma);
	if(Antialiasing>1)
	{
		msaaFramebuffer->Deactivate(deviceContext);
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Resolve MSAA Framebuffer")
		renderPlatform->Resolve(deviceContext,view->GetFramebuffer()->GetTexture(),msaaFramebuffer->GetTexture());
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	}
	else
		view->GetFramebuffer()->DeactivateDepth(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"3")
	crossplatform::Texture *depthTexture=NULL;
	if(Antialiasing>1)
		depthTexture	=msaaFramebuffer->GetDepthTexture();
	else
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
	// Suppose we want a linear depth texture? In that case, we make it here:
	if(cameraViewStruct.projection==crossplatform::LINEAR)
	{
		if(!linearDepthTexture)
		{
			linearDepthTexture=renderPlatform->CreateTexture();
		}
		if(!linearizeDepthEffect)
		{
			std::map<std::string,std::string> defines;
			defines["REVERSE_DEPTH"]="0";
			linearizeDepthEffect=renderPlatform->CreateEffect("hdr",defines);
		}
		// Now we will create a linear depth texture:
		SIMUL_GPU_PROFILE_START(deviceContext.platform_context,"Linearize depth buffer")
		// Have to make a non-MSAA texture, can't copy the AA across.
		linearDepthTexture->ensureTexture2DSizeAndFormat(renderPlatform,depthTexture->width,depthTexture->length,crossplatform::R_32_FLOAT,false,true,false,Antialiasing);
		{
			linearDepthTexture->activateRenderTarget(deviceContext);
			linearizeDepthEffect->Apply(deviceContext,linearizeDepthEffect->GetTechniqueByName("linearize_depth"),Antialiasing>1?"msaa":"main");
			if(Antialiasing>1)
				linearizeDepthEffect->SetTexture(deviceContext,"depthTextureMS",depthTexture);
			else
				linearizeDepthEffect->SetTexture(deviceContext,"depthTexture",depthTexture);
			crossplatform::Frustum f=crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
			static float cc=300000.f;
			linearizeDepthEffect->SetParameter("fullResDims",int2(depthTexture->width,depthTexture->length));
			linearizeDepthEffect->SetParameter("depthToLinFadeDistParams",vec4(deviceContext.viewStruct.proj[3*4+2],f.farZ,deviceContext.viewStruct.proj[2*4+2]*f.farZ,0.0f));
			renderPlatform->DrawQuad(deviceContext);
			linearizeDepthEffect->Unapply(deviceContext);
			linearDepthTexture->deactivateRenderTarget();
		}
		depthTexture=linearDepthTexture;
		deviceContext.viewStruct.depthTextureStyle=crossplatform::DISTANCE_FROM_NEAR_PLANE;
		SIMUL_GPU_PROFILE_END(pContext)
	}
	
	if(simulWeatherRenderer)
	{
		crossplatform::Viewport viewport={0,0,depthTexture->width,depthTexture->length,0.f,1.f};
		viewManager.DownscaleDepth(deviceContext
									,depthTexture
									,NULL
									,simulWeatherRenderer->GetAtmosphericDownscale()
									,simulWeatherRenderer->GetDownscale()
									,simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetMaxDistanceKm()*1000.0f
									,trueSkyRenderMode==clouds::MIXED_RESOLUTION);
	}
	if(trueSkyRenderMode==clouds::MIXED_RESOLUTION||trueSkyRenderMode==clouds::COMPOSITED_ATMOSPHERICS)
		RenderMixedResolutionSky(deviceContext,depthTexture,exposure,gamma);
	view->GetFramebuffer()->Deactivate(deviceContext);
	if(hdr&&simulHDRRenderer)
		simulHDRRenderer->Render(deviceContext,view->GetResolvedHDRBuffer(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	
	if(baseOpticsRenderer&&ShowFlares&&simulWeatherRenderer->GetBaseSkyRenderer())
	{
		simul::sky::float4 dir,light;
		math::Vector3 cam_pos	=GetCameraPosVector(deviceContext.viewStruct.view);
		dir						=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
		light					=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
		float occ				=simulWeatherRenderer->GetBaseSkyRenderer()->GetSunOcclusion();
		float exp				=(hdr?cameraViewStruct.exposure:1.f)*(1.f-occ);
		baseOpticsRenderer->RenderFlare(deviceContext,exp,NULL,dir,light);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void TrueSkyRenderer::RenderOverlays(crossplatform::DeviceContext &deviceContext,const crossplatform::CameraViewStruct &cameraViewStruct)
{
	if(!AllOsds)
		return;
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Overlays")
	crossplatform::MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer->IsValid())
	{
		static float x=0.35f,y=0.4f;
		renderPlatform->DrawCubemap(deviceContext,cubemapFramebuffer->GetTexture()	,-x		,y		,cameraViewStruct.exposure,cameraViewStruct.gamma);
		if(envmapFramebuffer->IsValid())
			renderPlatform->DrawCubemap(deviceContext,envmapFramebuffer->GetTexture()	,x		,y		,cameraViewStruct.exposure,cameraViewStruct.gamma);
	}
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->RenderOverlays(deviceContext);
		bool vertical_screen=(view->GetScreenHeight()>view->GetScreenWidth());
		crossplatform::Viewport v=renderPlatform->GetViewport(deviceContext,0);
		int W1=(int)v.w;
		int H1=(int)v.h;
		int W2=W1/2;
		int H2=H1/2;
		if (simulWeatherRenderer->GetShowCompositing())
		{
			RenderDepthBuffers(deviceContext,v,W2,0,W2,H2);
		}
		{
			char txt[40];
		//	sprintf(txt,"%4.4f %4.4f", view->pixelOffset.x,view->pixelOffset.y);
			math::Vector3 cam_pos	=GetCameraPosVector(deviceContext.viewStruct.view);
			float c=simulWeatherRenderer->GetEnvironment()->cloudKeyframer->GetCloudiness(cam_pos);
			sprintf(txt,"In cloud: %4.4f", c);	
			//renderPlatform->Print(deviceContext,16,16,txt);
		}
		if(oceanRenderer&&ShowWaterTextures)
		{
			oceanRenderer->RenderTextures(deviceContext,v.w,v.h);
		}
		if(ShowHDRTextures&&simulHDRRenderer)
		{
			simulHDRRenderer->RenderDebug(deviceContext,W2,H2,W2,H2);
		}
		if(ShowOSD&&simulWeatherRenderer->GetBaseCloudRenderer())
		{
			simulWeatherRenderer->GetBaseCloudRenderer()->RenderDebugInfo(deviceContext,W1,H1);
#ifdef _XBOX_ONE
			const char *txt=Profiler::GetGlobalProfiler().GetDebugText();
			renderPlatform->Print(deviceContext			,12	,12,txt);
#endif
		}
		std::string str="DirectX 11\n";
		renderPlatform->Print(deviceContext			,12	,12,str.c_str());
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void TrueSkyRenderer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Viewport viewport,int x0,int y0,int dx,int dy)
{
	crossplatform::MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
	crossplatform::Texture *depthTexture=NULL;
	if(Antialiasing>1)
		depthTexture	=msaaFramebuffer->GetDepthTexture();
	else
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
	view->RenderDepthBuffers(deviceContext,depthTexture,&viewport,x0,y0,dx,dy);
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->RenderFramebufferDepth(deviceContext,x0+dx/2	,y0	,dx/2,dy/2);
		//UtilityRenderer::DrawTexture(pContext,2*w	,0,w,l,(ID3D11ShaderResourceView*)simulWeatherRenderer->GetFramebufferTexture(view_id)	,1.f		);
		//renderPlatform->Print(pContext			,2.f*w	,0.f,"Near overlay");
	}
}

void TrueSkyRenderer::SaveScreenshot(const char *filename_utf8,int width,int height,float exposure,float gamma)
{
	std::string screenshotFilenameUtf8=filename_utf8;
	crossplatform::MixedResolutionView *view=viewManager.GetView(0);
	if(!view)
		return;
	try
	{
		if(width==0)
			width=view->GetScreenWidth();
		if(height==0)
			height=view->GetScreenHeight();
		crossplatform::BaseFramebuffer *fb=renderPlatform->CreateFramebuffer();
		fb->SetWidthAndHeight(width,height);
		fb->RestoreDeviceObjects(renderPlatform);
		crossplatform::DeviceContext deviceContext=renderPlatform->GetImmediateContext();
		fb->Activate(deviceContext);
		bool t=UseHdrPostprocessor;
		UseHdrPostprocessor=true;
	
		crossplatform::CameraOutputInterface *cam		=const_cast<crossplatform::CameraOutputInterface *>(cameras[0]);
		crossplatform::CameraViewStruct s0=cam->GetCameraViewStruct();
		crossplatform::CameraViewStruct s=s0;
		s.exposure=exposure;
		s.gamma=gamma/0.44f;
		cam->SetCameraViewStruct(s);
		AllOsds=false;
		Render(0,deviceContext.asD3D11DeviceContext());
		AllOsds=true;
		UseHdrPostprocessor=t;
		fb->Deactivate(deviceContext);
		cam->SetCameraViewStruct(s0);
		renderPlatform->SaveTexture(fb->GetTexture(),screenshotFilenameUtf8.c_str());
		SAFE_DELETE(fb);
	}
	catch(...)
	{
	}
}

void TrueSkyRenderer::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	if(demoOverlay)
		demoOverlay->InvalidateDeviceObjects();
	std::cout<<"TrueSkyRenderer::OnD3D11LostDevice"<<std::endl;
	Profiler::GetGlobalProfiler().Uninitialize();
	if(oceanRenderer)
		oceanRenderer->InvalidateDeviceObjects();
	clouds::TrueSkyRenderer::InvalidateDeviceObjects();
	renderPlatform=NULL;
}

void TrueSkyRenderer::RecompileShaders()
{
	clouds::TrueSkyRenderer::RecompileShaders();
	if(oceanRenderer)
		oceanRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
}

void TrueSkyRenderer::ReverseDepthChanged()
{
	if(renderPlatform)
		renderPlatform->SetReverseDepth(ReverseDepth);
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulHDRRenderer)
		simulHDRRenderer->SetReverseDepth(ReverseDepth);
	if(baseTerrainRenderer)
		baseTerrainRenderer->SetReverseDepth(ReverseDepth);
	if(oceanRenderer)
		oceanRenderer->SetReverseDepth(ReverseDepth);
	RecompileShaders();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m)
	:trueSkyRenderer(env,s,m)
{
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	trueSkyRenderer.InvalidateDeviceObjects();
}

void Direct3D11Renderer::OnD3D11CreateDevice	(ID3D11Device* pd3dDevice)
{
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	renderPlatformDx11.RestoreDeviceObjects(pd3dDevice);
	trueSkyRenderer.RestoreDeviceObjects(&renderPlatformDx11);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	trueSkyRenderer.InvalidateDeviceObjects();
	renderPlatformDx11.InvalidateDeviceObjects();
}

D3D_FEATURE_LEVEL Direct3D11Renderer::GetMinimumFeatureLevel() const
{
	return D3D_FEATURE_LEVEL_11_0;
}

int	Direct3D11Renderer::AddView(bool external_fb)
{
	return trueSkyRenderer.AddView(external_fb);
}

void Direct3D11Renderer::RemoveView			(int view_id)
{
	return trueSkyRenderer.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	return trueSkyRenderer.ResizeView(view_id,pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
}
void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	simul::base::SetGpuProfilingInterface(pContext,&simul::dx11::Profiler::GetGlobalProfiler());
	trueSkyRenderer.Render(view_id,pContext);
}