#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/Crossplatform/Material.h"
#include "Simul/Platform/Crossplatform/DemoOverlay.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Platform/DirectX11/SimulWeatherRendererDX11.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/DirectX11/OceanRenderer.h"
#include "Simul/Platform/DirectX11/SimulHDRRendererDX1x.h"
#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Camera/Camera.h"
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

TrueSkyRenderer::TrueSkyRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m):
		ShowFlares(true)
		,ShowWaterTextures(false)
		,ShowTerrain(true)
		,ShowMap(false)
		,trueSkyRenderMode(clouds::MIXED_RESOLUTION)
		,DepthBasedComposite(true)
		,UseHdrPostprocessor(true)
		,UseSkyBuffer(true)
		,ShowHDRTextures(false)
		,ShowLightVolume(false)
		,ShowGroundGrid(false)
		,ShowWater(true)
		,MakeCubemap(true)
		,ShowCubemaps(false)
		,PerformanceTest(DEFAULT)
		,ReverseDepth(false)
		,ShowOSD(false)
		,SkyBrightness(1.f)
		,Antialiasing(1)
		,SphericalHarmonicsBands(4)
		,renderPlatform(NULL)
		,cubemap_view_id(-1)
		,enabled(false)
		,lightProbesEffect(NULL)
		,linearizeDepthEffect(NULL)
		,baseOpticsRenderer(NULL)
		,simulWeatherRenderer(NULL)
		,simulHDRRenderer(NULL)
		,baseTerrainRenderer(NULL)
		,oceanRenderer(NULL)
#ifdef SIMUL_USE_SCENE
		,sceneRenderer(NULL)
#endif
		,msaaFramebuffer(NULL)
		,memoryInterface(m)
		,linearDepthTexture(NULL)
		,envmapFramebuffer(NULL)
		,AllOsds(true)
		,demoOverlay(NULL)
{
	sc;
	simulHDRRenderer		=::new(memoryInterface) SimulHDRRendererDX1x(128,128);
	simulWeatherRenderer	=::new(memoryInterface) SimulWeatherRendererDX11(env,memoryInterface);
	baseOpticsRenderer		=::new(memoryInterface) camera::BaseOpticsRenderer(memoryInterface);
	baseTerrainRenderer		=::new(memoryInterface) terrain::BaseTerrainRenderer(memoryInterface);
	baseTerrainRenderer->SetBaseSkyInterface(env->skyKeyframer);

	if((simul::base::GetFeatureLevel()&simul::base::EXPERIMENTAL)!=0)
	{
		oceanRenderer			=new(memoryInterface) OceanRenderer(env->seaKeyframer);
		oceanRenderer->SetBaseSkyInterface(env->skyKeyframer);
	}

#ifdef SIMUL_USE_SCENE
	if(sc)
		sceneRenderer			=new(memoryInterface) scene::BaseSceneRenderer(sc,renderPlatform);
#endif
	ReverseDepthChanged();
	
	cubemapFramebuffer.SetFormat(crossplatform::RGBA_16_FLOAT);
	cubemapFramebuffer.SetDepthFormat(crossplatform::D_32_FLOAT);
}

TrueSkyRenderer::~TrueSkyRenderer()
{
	InvalidateDeviceObjects();
#ifdef SIMUL_USE_SCENE
	del(sceneRenderer,memoryInterface);
#endif
	del(baseOpticsRenderer,memoryInterface);
	del(simulWeatherRenderer,memoryInterface);
	del(simulHDRRenderer,memoryInterface);
	del(baseTerrainRenderer,memoryInterface);
	del(oceanRenderer,memoryInterface);
	SAFE_DELETE(envmapFramebuffer);
}

void TrueSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!renderPlatform)
	{
		enabled=false;
		return;
	}
	enabled=true;
	SAFE_DELETE(envmapFramebuffer);
	envmapFramebuffer=new CubemapFramebuffer;//renderPlatform->CreateFramebuffer();
	envmapFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
	envmapFramebuffer->SetDepthFormat(crossplatform::UNKNOWN);
	SAFE_DELETE(msaaFramebuffer);
	msaaFramebuffer=renderPlatform->CreateFramebuffer();
	msaaFramebuffer->RestoreDeviceObjects(renderPlatform);
	//Set a global device pointer for use by various classes.
	viewManager.RestoreDeviceObjects(renderPlatform);
	lightProbeConstants.RestoreDeviceObjects(renderPlatform);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(renderPlatform);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(renderPlatform);
	if(baseOpticsRenderer)
		baseOpticsRenderer->RestoreDeviceObjects(renderPlatform);
	if(baseTerrainRenderer)
		baseTerrainRenderer->RestoreDeviceObjects(renderPlatform);
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(renderPlatform);
	cubemapFramebuffer.SetAsCubemap(32);
	cubemapFramebuffer.RestoreDeviceObjects(renderPlatform);
	envmapFramebuffer->SetAsCubemap(8);
	envmapFramebuffer->RestoreDeviceObjects(renderPlatform);
	//envmapFramebuffer->Clear(pContext,0.f,0.f,0.f,1.f,0.f);
	if(renderPlatform)
	{
		crossplatform::DeviceContext deviceContext=renderPlatform->GetImmediateContext();
		cubemapFramebuffer.Clear(deviceContext, 0.5f, 0.5f, 0.5, 0.f, 0.f);
		envmapFramebuffer->Clear(deviceContext, 0.5f, 0.5f, 0.5, 0.f, 0.f);
	}
	//if(!demoOverlay)
	//	demoOverlay=new crossplatform::DemoOverlay();
	if(demoOverlay)
	demoOverlay->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

int	TrueSkyRenderer::AddView				(bool external_fb)
{
	int view_id=viewManager.AddView(external_fb);
	viewManager.GetView(view_id)->RestoreDeviceObjects(renderPlatform);
	return view_id;
}

void TrueSkyRenderer::RemoveView			(int view_id)
{
	viewManager.RemoveView(view_id);
}

void TrueSkyRenderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	crossplatform::MixedResolutionView *view			=viewManager.GetView(view_id);
	if(view)
	{	
		view->RestoreDeviceObjects(renderPlatform);
		// RVK: Downscale here to get a closeup view of small-scale pixel effects.
		view->SetResolution(pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
	}
}

void TrueSkyRenderer::EnsureCorrectBufferSizes(int view_id)
{
	crossplatform::MixedResolutionView *view			=viewManager.GetView(view_id);
	if(!view)
		return;
	static bool lockx=false,locky=false;
	static int magnify=1;
	// Must have a whole number of full-res pixels per low-res pixel.
	int W=view->GetScreenWidth(),H=view->GetScreenHeight();
	W=W/magnify;
	H=H/magnify;
 	if(simulWeatherRenderer)
	{
		if(lockx)
		{
			int s					=simulWeatherRenderer->GetDownscale();
			int w					=W/s;
			W						=w*s;
		}
		if(locky)
		{
			int s					=simulWeatherRenderer->GetDownscale();
			int h					=H/s;
			H						=h*s;
		}
		if(PerformanceTest==FORCE_1920_1080)
		{
			W=1920;
			H=1080;
		}
		if(PerformanceTest==FORCE_2560_1600)
		{
			W=2560;
			H=1600;
		}
		else if(PerformanceTest==FORCE_3840_2160)
		{
			W=3840;
			H=2160;
		}
		view->SetResolution(W,H);
//		simulWeatherRenderer->SetScreenSize(view_id,W,H);
	}
	W=view->GetScreenWidth();
	H=view->GetScreenHeight();
	if(view->viewType==crossplatform::OCULUS_VR)
		W=view->GetScreenWidth()/2;
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(W,H);
	view->GetFramebuffer()->SetWidthAndHeight(W,H);
	// This buffer won't be used for multisampling, we will RESOLVE the msaa buffer to this one.
	view->GetFramebuffer()->SetAntialiasing(1);
}

void TrueSkyRenderer::RenderCubemap(crossplatform::DeviceContext &parentDeviceContext,const float *cam_pos)
{
	simul::math::Matrix4x4 view_matrices[6];
	camera::MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
ERRNO_CHECK
	if(cubemap_view_id<0)
		cubemap_view_id=viewManager.AddView(false);
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=parentDeviceContext.platform_context;
	deviceContext.renderPlatform	=parentDeviceContext.renderPlatform;
	deviceContext.viewStruct.view_id=cubemap_view_id;
	if(baseTerrainRenderer)
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			baseTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture(cam_pos));
	// We want to limit the number of raytrace steps for clouds in the cubemap.
	if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
	{
		simulWeatherRenderer->GetBaseCloudRenderer()->SetMaxSlices(cubemap_view_id,20);
	}
	//for(int i=0;i<6;i++)
	static int i=0;
	i++;
	i=i%6;
	{
ERRNO_CHECK
		cubemapFramebuffer.SetCubeFace(i);
		cubemapFramebuffer.Clear(deviceContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		cubemapFramebuffer.Activate(deviceContext);
		static float nearPlane	=10.f;
		static float farPlane	=200000.f;
ERRNO_CHECK
		if(ReverseDepth)
			deviceContext.viewStruct.proj=simul::camera::Camera::MakeDepthReversedProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
		else
			deviceContext.viewStruct.proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
ERRNO_CHECK
		deviceContext.viewStruct.view_id=cubemap_view_id;
		deviceContext.viewStruct.view	=view_matrices[i];
		if(simulWeatherRenderer)
		{
			float time=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetTime();
			for(int i=0;i<simulWeatherRenderer->GetEnvironment()->cloudKeyframer->GetNumLightningBolts(time);i++)
			{
				const simul::clouds::LightningRenderInterface *lightningRenderInterface=simulWeatherRenderer->GetEnvironment()->cloudKeyframer->GetLightningBolt(time,i);
				if(!lightningRenderInterface)
					continue;
				simul::terrain::LightningIllumination lightningIllumination;
				simul::clouds::LightningProperties props	=simulWeatherRenderer->GetEnvironment()->cloudKeyframer->GetLightningProperties(time,i);
				lightningIllumination.colour				=vec3(0,0,0);
				const simul::clouds::CloudKeyframer *cloudKeyframer=simulWeatherRenderer->GetEnvironment()->cloudKeyframer;
				for(int i=0;i<cloudKeyframer->GetNumLightningBolts(time);i++)
				{
					const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,i);
					if(!lightningRenderInterface)
						continue;
					simul::clouds::LightningProperties props	=cloudKeyframer->GetLightningProperties(time,i);
					if(!props.numLevels)
						continue;
					float brightness							=lightningRenderInterface->GetBrightness(props,time,0,0);
					lightningIllumination.centre	=props.startX;
					lightningIllumination.colour	=props.colour;
					lightningIllumination.colour	*=brightness;
				}
				if(baseTerrainRenderer)
					baseTerrainRenderer->SetLightningProperties(lightningIllumination);
			}
		}
		if(baseTerrainRenderer&&ShowTerrain)
			baseTerrainRenderer->Render(deviceContext,1.f);
		if(oceanRenderer&&ShowWater)
		{
			//oceanRenderer->Render(deviceContext,1.f);
		}
		cubemapFramebuffer.DeactivateDepth(deviceContext);
		if(simulWeatherRenderer)
		{
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
			simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,true,1.f,1.f,false,cubemapFramebuffer.GetDepthTexture(),relativeViewportTextureRegionXYWH,true,vec2(0,0));
		}
		static const char *txt[]={	"+X",
									"-X",
									"+Y",
									"-Y",
									"+Z",
									"-Z"};
		//renderPlatform->Print(deviceContext,16,16,txt[i]);
		cubemapFramebuffer.Deactivate(deviceContext);
	//	renderPlatform->DrawDepth(deviceContext,0,0,64,64,cubemapFramebuffer.GetDepthTexture());
	}
ERRNO_CHECK
}

void TrueSkyRenderer::RenderEnvmap(crossplatform::DeviceContext &deviceContext)
{
	if(!lightProbesEffect)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"RenderEnvmap CalcSphericalHarmonics")
	cubemapFramebuffer.SetBands(SphericalHarmonicsBands);
	cubemapFramebuffer.CalcSphericalHarmonics(deviceContext);
	math::Matrix4x4 invViewProj;
	math::Matrix4x4 view_matrices[6];
	float cam_pos[]={0,0,0};
	crossplatform::EffectTechnique *tech=lightProbesEffect->GetTechniqueByName("irradiance_map");
	camera::MakeCubeMatrices(view_matrices,cam_pos,false);
	SIMUL_COMBINED_PROFILE_END(pContext)
	// For each face, 
	SIMUL_COMBINED_PROFILE_START(pContext,"RenderEnvmap draw")
	static int i=0;
	i++;
	i=i%6;
	//for(int i=0;i<6;i++)
	{
		envmapFramebuffer->SetCubeFace(i);
		envmapFramebuffer->Activate(deviceContext);
		math::Matrix4x4 cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,1.f,200000.f);
		{
			camera::MakeInvViewProjMatrix(invViewProj,(const float*)&view_matrices[i],cube_proj);
			lightProbeConstants.invViewProj	=invViewProj;
			lightProbeConstants.numSHBands	=SphericalHarmonicsBands;
			lightProbeConstants.alpha		=0.05f;
			lightProbeConstants.Apply(deviceContext);
			cubemapFramebuffer.GetSphericalHarmonics().Apply(deviceContext, lightProbesEffect,"basisBuffer");
			lightProbesEffect->Apply(deviceContext,tech,0);
			renderPlatform->DrawQuad(deviceContext);
			lightProbesEffect->SetTexture(deviceContext,"basisBuffer"	,NULL);
			lightProbesEffect->Unapply(deviceContext);
		}
		envmapFramebuffer->Deactivate(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
}

// The elements in the main colour/depth buffer, with depth test and optional MSAA
void TrueSkyRenderer::RenderDepthElements(crossplatform::DeviceContext &deviceContext
									 ,float exposure
									 ,float gamma)
{
	if(baseTerrainRenderer&&ShowTerrain)
	{
		math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
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
				,const camera::CameraViewStruct &cameraViewStruct)
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
	const camera::CameraOutputInterface *cam		=cameras[deviceContext.viewStruct.view_id];
	//left eye
	if(cam)
	{
		deviceContext.viewStruct.proj=cam->MakeStereoProjectionMatrix(simul::camera::LEFT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
		deviceContext.viewStruct.view=cam->MakeStereoViewMatrix(simul::camera::LEFT_EYE);
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
		deviceContext.viewStruct.proj	=cam->MakeStereoProjectionMatrix(simul::camera::RIGHT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
		deviceContext.viewStruct.view	=cam->MakeStereoViewMatrix(simul::camera::RIGHT_EYE);
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
	if(!enabled)
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
	D3D11_VIEWPORT				viewport;
	memset(&viewport,0,sizeof(D3D11_VIEWPORT));
	UINT numv=1;
	pContext->RSGetViewports(&numv, &viewport);
	view->SetResolution((int)viewport.Width,(int)viewport.Height);
	EnsureCorrectBufferSizes(view_id);
	const camera::CameraOutputInterface *cam		=cameras[view_id];
	SIMUL_ASSERT(cam!=NULL);
	const camera::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	
	SetReverseDepth(cameraViewStruct.projection==camera::DEPTH_REVERSE);
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
		const float *cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
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

void TrueSkyRenderer::RenderStandard(crossplatform::DeviceContext &deviceContext,const camera::CameraViewStruct &cameraViewStruct)
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
		simulWeatherRenderer->RenderCelestialBackground(deviceContext,NULL,exposure);
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
	if(cameraViewStruct.projection==camera::LINEAR)
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
			camera::Frustum f=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
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

void TrueSkyRenderer::RenderOverlays(crossplatform::DeviceContext &deviceContext,const camera::CameraViewStruct &cameraViewStruct)
{
	if(!AllOsds)
		return;
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Overlays")
	crossplatform::MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer.IsValid())
	{
		static float x=0.35f,y=0.4f;
		renderPlatform->DrawCubemap(deviceContext,cubemapFramebuffer.GetTexture()	,-x		,y		,cameraViewStruct.exposure,cameraViewStruct.gamma);
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
			renderPlatform->Print(deviceContext,16,16,txt);
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
	
		camera::CameraOutputInterface *cam		=const_cast<camera::CameraOutputInterface *>(cameras[0]);
		camera::CameraViewStruct s0=cam->GetCameraViewStruct();
		camera::CameraViewStruct s=s0;
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
#ifdef SIMUL_USE_SCENE
	if(sceneRenderer)
		sceneRenderer->InvalidateDeviceObjects();
#endif
	if(demoOverlay)
		demoOverlay->InvalidateDeviceObjects();
	std::cout<<"TrueSkyRenderer::OnD3D11LostDevice"<<std::endl;
	Profiler::GetGlobalProfiler().Uninitialize();
	SAFE_DELETE(msaaFramebuffer);
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(baseOpticsRenderer)
		baseOpticsRenderer->InvalidateDeviceObjects();
	if(baseTerrainRenderer)
		baseTerrainRenderer->InvalidateDeviceObjects();
	if(oceanRenderer)
		oceanRenderer->InvalidateDeviceObjects();
#ifdef SIMUL_USE_SCENE
	if(sceneRenderer)
		sceneRenderer->InvalidateDeviceObjects();
#endif
	viewManager.Clear();
	cubemapFramebuffer.InvalidateDeviceObjects();
	SAFE_DELETE(lightProbesEffect);
	SAFE_DELETE(linearizeDepthEffect);
	SAFE_DELETE(linearDepthTexture);
	viewManager.InvalidateDeviceObjects();
	renderPlatform=NULL;
}

void TrueSkyRenderer::RecompileShaders()
{
	cubemapFramebuffer.RecompileShaders();
	if(renderPlatform)
		renderPlatform->RecompileShaders();
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(baseOpticsRenderer)
		baseOpticsRenderer->RecompileShaders();
	if(baseTerrainRenderer)
		baseTerrainRenderer->RecompileShaders();
	if(oceanRenderer)
		oceanRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();

	std::map<std::string,std::string> defines;
	//["REVERSE_DEPTH"]		=ReverseDepth?"1":"0";
	//defines["NUM_AA_SAMPLES"]		=base::stringFormat("%d",Antialiasing);
	viewManager.RecompileShaders();
	SAFE_DELETE(lightProbesEffect);
	SAFE_DELETE(linearizeDepthEffect);
	if(renderPlatform)
	{
		lightProbesEffect=renderPlatform->CreateEffect("light_probes.fx",defines);
		lightProbeConstants.LinkToEffect(lightProbesEffect,"LightProbeConstants");
	}
}

void TrueSkyRenderer::ReloadTextures()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->ReloadTextures();
}

const char *TrueSkyRenderer::GetDebugText() const
{
	if(!ShowOSD)
		return " ";
	static std::string str;
	str="DirectX 11\n";
	//if(simulWeatherRenderer)
//		str+=simulWeatherRenderer->GetDebugText();
	str+=Profiler::GetGlobalProfiler().GetDebugText();
	return str.c_str();
}

void TrueSkyRenderer::SetViewType(int view_id,crossplatform::ViewType vt)
{
	crossplatform::MixedResolutionView *v=viewManager.GetView(view_id);
	if(!v)
		return;
	v->viewType=vt;
}

void TrueSkyRenderer::SetCamera(int view_id,const simul::camera::CameraOutputInterface *c)
{
	cameras[view_id]=c;
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

void TrueSkyRenderer::AntialiasingChanged()
{
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
	return trueSkyRenderer.ResizeView(view_id,pBackBufferSurfaceDesc);
}
void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	simul::base::SetGpuProfilingInterface(pContext,&simul::dx11::Profiler::GetGlobalProfiler());
	trueSkyRenderer.Render(view_id,pContext);
}