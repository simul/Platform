#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Platform/DirectX11/SimulWeatherRendererDX11.h"
#include "Simul/Platform/DirectX11/TerrainRenderer.h"
#include "Simul/Platform/DirectX11/OceanRenderer.h"
#include "Simul/Platform/DirectX11/SimulCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulHDRRendererDX1x.h"
#include "Simul/Platform/DirectX11/Simul2DCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulOpticsRendererDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/pi.h"
#include "D3dx11effect.h"
#ifdef SIMUL_USE_SCENE
#include "Simul/Scene/BaseSceneRenderer.h"
#endif
using namespace simul;
using namespace dx11;

Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m):
		ShowCloudCrossSections(false)
		,ShowFlares(true)
		,Show2DCloudTextures(false)
		,ShowWaterTextures(false)
		,ShowFades(false)
		,ShowTerrain(true)
		,ShowMap(false)
		,UseMixedResolution(true)
		,UseHdrPostprocessor(true)
		,UseSkyBuffer(true)
		,ShowCompositing(false)
		,ShowLightVolume(false)
		,CelestialDisplay(false)
		,ShowWater(true)
		,MakeCubemap(true)
		,ShowCubemaps(false)
		,ShowRainTextures(false)
		,ReverseDepth(true)
		,ShowOSD(false)
		,SkyBrightness(1.f)
		,Antialiasing(1)
		,SphericalHarmonicsBands(4)
		,cubemap_view_id(-1)
		,enabled(false)
		,m_pd3dDevice(NULL)
		,lightProbesEffect(NULL)
		,simulOpticsRenderer(NULL)
		,simulWeatherRenderer(NULL)
		,simulHDRRenderer(NULL)
		,simulTerrainRenderer(NULL)
		,oceanRenderer(NULL)
#ifdef SIMUL_USE_SCENE
		,sceneRenderer(NULL)
#endif
		,memoryInterface(m)
{
	simulHDRRenderer		=::new(memoryInterface) SimulHDRRendererDX1x(128,128);
	simulWeatherRenderer	=::new(memoryInterface) SimulWeatherRendererDX11(env,memoryInterface);
	simulOpticsRenderer		=::new(memoryInterface) SimulOpticsRendererDX1x(memoryInterface);
	simulTerrainRenderer	=::new(memoryInterface) TerrainRenderer(memoryInterface);
	simulTerrainRenderer->SetBaseSkyInterface(env->skyKeyframer);

	oceanRenderer			=new(memoryInterface) OceanRenderer(env->seaKeyframer);
	oceanRenderer->SetBaseSkyInterface(env->skyKeyframer);
	
#ifdef SIMUL_USE_SCENE
	if(sc)
		sceneRenderer			=new(memoryInterface) scene::BaseSceneRenderer(sc,&renderPlatformDx11);
#endif
	ReverseDepthChanged();
	cubemapFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	cubemapFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	
	envmapFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	envmapFramebuffer.SetDepthFormat(DXGI_FORMAT_UNKNOWN);
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	OnD3D11LostDevice();
#ifdef SIMUL_USE_SCENE
	del(sceneRenderer,memoryInterface);
#endif
	del(simulOpticsRenderer,memoryInterface);
	del(simulWeatherRenderer,memoryInterface);
	del(simulHDRRenderer,memoryInterface);
	del(simulTerrainRenderer,memoryInterface);
	del(oceanRenderer,memoryInterface);
}


D3D_FEATURE_LEVEL Direct3D11Renderer::GetMinimumFeatureLevel() const
{
	return D3D_FEATURE_LEVEL_11_0;
}

void Direct3D11Renderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
	m_pd3dDevice=pd3dDevice;
	if(!pd3dDevice)
	{
		enabled=false;
		return;
	}
	enabled=true;
	renderPlatformDx11.RestoreDeviceObjects(pd3dDevice);
	//Set a global device pointer for use by various classes.
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	simul::dx11::UtilityRenderer::RestoreDeviceObjects(&renderPlatformDx11);
	viewManager.RestoreDeviceObjects(&renderPlatformDx11);
	lightProbeConstants.RestoreDeviceObjects(m_pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(&renderPlatformDx11);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(&renderPlatformDx11);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(&renderPlatformDx11);
	cubemapFramebuffer.SetWidthAndHeight(32,32);
	cubemapFramebuffer.RestoreDeviceObjects(&renderPlatformDx11);
	envmapFramebuffer.SetWidthAndHeight(8,8);
	envmapFramebuffer.RestoreDeviceObjects(&renderPlatformDx11);
	RecompileShaders();
}

int	Direct3D11Renderer::AddView				(bool external_fb)
{
	int view_id=viewManager.AddView(external_fb);
	viewManager.GetView(view_id)->RestoreDeviceObjects(&renderPlatformDx11);
	return view_id;
}

void Direct3D11Renderer::RemoveView			(int view_id)
{
	viewManager.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	MixedResolutionView *view			=viewManager.GetView(view_id);
	if(view)
	{	
		view->RestoreDeviceObjects(&renderPlatformDx11);
		// RVK: Downscale here to get a closeup view of small-scale pixel effects.
		view->SetResolution(pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
	}
}

void Direct3D11Renderer::EnsureCorrectBufferSizes(int view_id)
{
	MixedResolutionView *view			=viewManager.GetView(view_id);
	if(!view)
		return;
	static bool lockx=false,locky=false;
	// Must have a whole number of full-res pixels per low-res pixel.
	int W=view->GetScreenWidth(),H=view->GetScreenHeight();
 	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetScreenSize(view_id,view->GetScreenWidth(),view->GetScreenHeight());
		if(lockx)
		{
			int s					=simulWeatherRenderer->GetDownscale();
			int w					=(W+s-1)/s;
			W						=w*s;
			view->SetResolution(W,H);
		}
		if(locky)
		{
			int s					=simulWeatherRenderer->GetDownscale();
			int h					=(H+s-1)/s;
			H						=h*s;
			view->SetResolution(W,H);
		}
	}
	W=view->GetScreenWidth();
	H=view->GetScreenHeight();
	if(view->viewType==OCULUS_VR)
		W=view->GetScreenWidth()/2;
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(W,H);
	view->GetFramebuffer()->SetWidthAndHeight(W,H);
	view->GetFramebuffer()->SetAntialiasing(Antialiasing);
}

void Direct3D11Renderer::RenderCubemap(crossplatform::DeviceContext &parentDeviceContext,const float *cam_pos)
{
	simul::math::Matrix4x4 view_matrices[6];
	camera::MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
ERRNO_CHECK
	if(cubemap_view_id<0)
		cubemap_view_id=viewManager.AddView(false);
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=parentDeviceContext.asD3D11DeviceContext();
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=cubemap_view_id;
	cubemapFramebuffer.Clear(deviceContext.platform_context,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	if(simulTerrainRenderer)
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture(cam_pos));
	for(int i=0;i<6;i++)
	{
ERRNO_CHECK
		cubemapFramebuffer.SetCurrentFace(i);
		cubemapFramebuffer.Activate(deviceContext);
		float nearPlane=1.f;
		float farPlane=200000.f;
ERRNO_CHECK
		if(ReverseDepth)
			deviceContext.viewStruct.proj=simul::camera::Camera::MakeDepthReversedProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
		else
			deviceContext.viewStruct.proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
ERRNO_CHECK
		deviceContext.viewStruct.view_id=cubemap_view_id;
		deviceContext.viewStruct.view	=view_matrices[i];
		if(simulTerrainRenderer)
			simulTerrainRenderer->Render(deviceContext,1.f);
		cubemapFramebuffer.DeactivateDepth(deviceContext.platform_context);
		if(simulWeatherRenderer)
		{
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
			simulWeatherRenderer->RenderSkyAsOverlay(deviceContext
				,true,1.f,false,cubemapFramebuffer.GetDepthTexture(),NULL,relativeViewportTextureRegionXYWH,true);
		}
		static const char *txt[]={"+X",
								"-X",
								"+Y",
								"-Y",
								"+Z",
								"-Z"};
		//renderPlatformDx11.Print(deviceContext,16,16,txt[i]);
		cubemapFramebuffer.Deactivate(deviceContext.platform_context);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetCubemapTexture(envmapFramebuffer.GetColorTex());
ERRNO_CHECK
	if(oceanRenderer)
		oceanRenderer->SetCubemapTexture(cubemapFramebuffer.GetColorTex());
ERRNO_CHECK
}

void Direct3D11Renderer::RenderEnvmap(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"RenderEnvmap CalcSphericalHarmonics")
	cubemapFramebuffer.SetBands(SphericalHarmonicsBands);
	cubemapFramebuffer.CalcSphericalHarmonics(deviceContext);
	envmapFramebuffer.Clear(pContext,0.f,1.f,0.f,1.f,0.f);
	math::Matrix4x4 invViewProj;
	math::Matrix4x4 view_matrices[6];
	float cam_pos[]={0,0,0};
	ID3DX11EffectTechnique *tech=lightProbesEffect->GetTechniqueByName("irradiance_map");
	camera::MakeCubeMatrices(view_matrices,cam_pos,false);
	SIMUL_COMBINED_PROFILE_END(pContext)
	// For each face, 
	SIMUL_COMBINED_PROFILE_START(pContext,"RenderEnvmap draw")
	for(int i=0;i<6;i++)
	{
		envmapFramebuffer.SetCurrentFace(i);
		envmapFramebuffer.Activate(deviceContext);
		math::Matrix4x4 cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,1.f,200000.f);
		{
			camera::MakeInvViewProjMatrix(invViewProj,(const float*)&view_matrices[i],cube_proj);
			lightProbeConstants.invViewProj	=invViewProj;
			lightProbeConstants.numSHBands	=SphericalHarmonicsBands;
			lightProbeConstants.Apply(deviceContext);
			simul::dx11::setTexture(lightProbesEffect,"basisBuffer"	,cubemapFramebuffer.GetSphericalHarmonics().shaderResourceView);
			ApplyPass(pContext,tech->GetPassByIndex(0));
			UtilityRenderer::DrawQuad(pContext);
			simul::dx11::setTexture(lightProbesEffect,"basisBuffer"	,NULL);
			ApplyPass(pContext,tech->GetPassByIndex(0));
		}
		envmapFramebuffer.Deactivate(pContext);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
}


void Direct3D11Renderer::RenderScene(crossplatform::DeviceContext &deviceContext
									 ,simul::clouds::BaseWeatherRenderer *w
									 ,float exposure
									 ,float gamma)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"RenderScene")
#if 1
	MixedResolutionView *view=viewManager.GetView(deviceContext.viewStruct.view_id);
	if(simulTerrainRenderer&&ShowTerrain)
	{
		math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view,false);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture(cam_pos));
		simulTerrainRenderer->Render(deviceContext,1.f);	
	}
#ifdef SIMUL_USE_SCENE
	if(sceneRenderer)
		sceneRenderer->Render(deviceContext);
#endif
	if(oceanRenderer&&ShowWater)
	{
		oceanRenderer->SetMatrices(deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		oceanRenderer->Render(pContext,1.f);
		if(oceanRenderer->GetShowWireframes())
			oceanRenderer->RenderWireframe(pContext);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->RenderCelestialBackground(deviceContext,exposure);
	if(simulHDRRenderer&&UseHdrPostprocessor)
		view->GetFramebuffer()->DeactivateDepth(pContext);
	else
		view->GetFramebuffer()->Deactivate(pContext);
	if(simulWeatherRenderer)
	{
		int s=simulWeatherRenderer->GetDownscale();
		viewManager.DownscaleDepth(deviceContext,s,simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetMaxDistanceKm()*1000.0f);
		simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
	
		crossplatform::Texture* skyBufferDepthTex = (UseSkyBuffer)? &view->lowResDepthTexture : &view->hiResDepthTexture;
	
		if(UseMixedResolution)
			simulWeatherRenderer->RenderMixedResolution(deviceContext,false,exposure,gamma,view->GetFramebuffer()->GetDepthTexture()
				,&view->hiResDepthTexture,skyBufferDepthTex,relativeViewportTextureRegionXYWH);
		else
			simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,false,exposure,UseSkyBuffer
				,view->GetFramebuffer()->GetDepthTexture(),view->GetFramebuffer()->GetDepthTexture()
				,relativeViewportTextureRegionXYWH,true);
		if(simulHDRRenderer&&UseHdrPostprocessor)
			view->GetFramebuffer()->ActivateDepth(deviceContext);
		simulWeatherRenderer->RenderLightning(deviceContext,&view->hiResDepthTexture,relativeViewportTextureRegionXYWH,simulWeatherRenderer->GetCloudDepthTexture(deviceContext.viewStruct.view_id));
		simulWeatherRenderer->DoOcclusionTests(deviceContext);
		simulWeatherRenderer->RenderPrecipitation(deviceContext,&view->hiResDepthTexture,relativeViewportTextureRegionXYWH);
		if(simulOpticsRenderer&&ShowFlares&&simulWeatherRenderer->GetSkyRenderer())
		{
			simul::sky::float4 dir,light;
			math::Vector3 cam_pos=GetCameraPosVector(deviceContext.viewStruct.view);
			dir			=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			light		=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
			float occ	=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
			float exp	=(simulHDRRenderer?exposure:1.f)*(1.f-occ);
			void *moistureTexture=NULL;
			if(simulWeatherRenderer->GetBasePrecipitationRenderer())
				moistureTexture=simulWeatherRenderer->GetBasePrecipitationRenderer()->GetMoistureTexture();
			simulOpticsRenderer->RenderFlare(deviceContext,exp,moistureTexture,dir,light);
		}
	}
#endif
	SIMUL_COMBINED_PROFILE_END(pContext)
}

void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	if(!enabled)
		return;
	simul::base::SetGpuProfilingInterface(pContext,&simul::dx11::Profiler::GetGlobalProfiler());
	MixedResolutionView *view=viewManager.GetView(view_id);
	if(!view)
		return;
	SIMUL_COMBINED_PROFILE_START(pContext,"PreRender")
	D3D11_VIEWPORT				viewport;
	memset(&viewport,0,sizeof(D3D11_VIEWPORT));
	UINT numv=1;
	pContext->RSGetViewports(&numv, &viewport);
	view->SetResolution((int)viewport.Width,(int)viewport.Height);
	EnsureCorrectBufferSizes(view_id);
	const camera::CameraOutputInterface *cam		=cameras[view_id];
	SIMUL_ASSERT(cam!=NULL);
	const camera::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=pContext;
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=view_id;
	
	SetReverseDepth(cameraViewStruct.ReverseDepth);
	if(cam)
	{
		float aspect=(float)view->GetScreenWidth()/(float)view->GetScreenHeight();
		if(ReverseDepth)
			deviceContext.viewStruct.proj=cam->MakeDepthReversedProjectionMatrix(aspect);
		else
			deviceContext.viewStruct.proj=cam->MakeProjectionMatrix(aspect);
		deviceContext.viewStruct.view=cam->MakeViewMatrix();
	}
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->PreRenderUpdate(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
	if(view->viewType==OCULUS_VR)
	{
		D3D11_VIEWPORT				viewport;
		memset(&viewport,0,sizeof(D3D11_VIEWPORT));
		viewport.MinDepth			=0.0f;
		viewport.MaxDepth			=1.0f;
		viewport.Width				=(float)view->GetScreenWidth()/2;
		viewport.Height				=(float)view->GetScreenHeight();
		pContext->RSSetViewports(1, &viewport);
		//left eye
		if(cam)
		{
			deviceContext.viewStruct.proj=cam->MakeStereoProjectionMatrix(simul::camera::LEFT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
			deviceContext.viewStruct.view=cam->MakeStereoViewMatrix(simul::camera::LEFT_EYE);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->GetFramebuffer()->SetAntialiasing(Antialiasing);
			view->GetFramebuffer()->Activate(deviceContext);
			view->GetFramebuffer()->Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		}
		RenderScene(deviceContext,simulWeatherRenderer,cameraViewStruct.exposure,cameraViewStruct.gamma);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->GetFramebuffer()->Deactivate(pContext);
			simulHDRRenderer->RenderWithOculusCorrection(deviceContext,view->GetFramebuffer()->GetColorTex(),cameraViewStruct.exposure,cameraViewStruct.gamma,0.f);
		}
		//right eye
		if(cam)
		{
			deviceContext.viewStruct.proj	=cam->MakeStereoProjectionMatrix(simul::camera::RIGHT_EYE,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
			deviceContext.viewStruct.view	=cam->MakeStereoViewMatrix(simul::camera::RIGHT_EYE);
		}
		viewport.TopLeftX			=view->GetScreenWidth()/2.f;
		pContext->RSSetViewports(1, &viewport);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->GetFramebuffer()->SetAntialiasing(Antialiasing);
			view->GetFramebuffer()->Activate(deviceContext);
			view->GetFramebuffer()->Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		}
		RenderScene(deviceContext,simulWeatherRenderer,cameraViewStruct.exposure,cameraViewStruct.gamma);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->GetFramebuffer()->Deactivate(pContext);
			simulHDRRenderer->RenderWithOculusCorrection(deviceContext,view->GetFramebuffer()->GetColorTex(),cameraViewStruct.exposure,cameraViewStruct.gamma,1.f);
		}
		return;
	}
	SIMUL_COMBINED_PROFILE_START(pContext,"Render")

	if(MakeCubemap)
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Cubemap")
		const float *cam_pos=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view);
		RenderCubemap(deviceContext,cam_pos);
		SIMUL_COMBINED_PROFILE_END(pContext)
		SIMUL_COMBINED_PROFILE_START(pContext,"Envmap")
		RenderEnvmap(deviceContext);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	if(UseHdrPostprocessor)
	{
		view->GetFramebuffer()->SetAntialiasing(Antialiasing);
		view->GetFramebuffer()->Activate(deviceContext);
		SIMUL_COMBINED_PROFILE_START(pContext,"Clear")
		view->GetFramebuffer()->Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	RenderScene(deviceContext,simulWeatherRenderer,UseHdrPostprocessor?1.f:cameraViewStruct.exposure,UseHdrPostprocessor?1.f:cameraViewStruct.gamma);
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer.IsValid())
	{
		static float x=0.35f,y=0.4f;
		UtilityRenderer::DrawCubemap(deviceContext,(ID3D11ShaderResourceView*)cubemapFramebuffer.GetColorTex(),-0.7f,0.7f);
		UtilityRenderer::DrawCubemap(deviceContext,(ID3D11ShaderResourceView*)cubemapFramebuffer.GetColorTex(),-x,y);
		//UtilityRenderer::DrawCubemap(deviceContext,(ID3D11ShaderResourceView*)cubemapFramebuffer.GetColorTex(),-0.4f,0.45f);
		UtilityRenderer::DrawCubemap(deviceContext,(ID3D11ShaderResourceView*)envmapFramebuffer.GetColorTex(),x,y);
	}
	if(UseHdrPostprocessor)
	{
		view->GetFramebuffer()->Deactivate(pContext);
		view->ResolveFramebuffer(deviceContext);
		if(simulHDRRenderer)
			simulHDRRenderer->Render(deviceContext,view->GetResolvedHDRBuffer(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	}
	SIMUL_COMBINED_PROFILE_START(pContext,"Overlays")
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(deviceContext);
		simul::dx11::UtilityRenderer::SetScreenSize(view->GetScreenWidth(),view->GetScreenHeight());
		bool vertical_screen=(view->GetScreenHeight()>view->GetScreenWidth()/2);
		if(ShowFades&&simulWeatherRenderer->GetSkyRenderer())
		{
			int x0=view->GetScreenWidth()/4;
			int y0=8;
			int w=view->GetScreenWidth()/4;
			if(vertical_screen)
			{
				x0=8;
				y0=view->GetScreenHeight()/2;
				w=view->GetScreenWidth()/2;
			}
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(deviceContext,x0,y0,w,view->GetScreenHeight()/2);
		}
		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer())
		{
			int w=view->GetScreenWidth()/4;
			if(vertical_screen)
				w=view->GetScreenWidth()/2;
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(deviceContext,0,0,w,view->GetScreenHeight()/2);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(deviceContext,0,0,w,view->GetScreenHeight()/2);
		}
		if(ShowCompositing)
		{
			RenderDepthBuffers(deviceContext,view->GetScreenWidth()/2,0,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(deviceContext,0,0,view->GetScreenWidth(),view->GetScreenHeight());
		}
		if(simulWeatherRenderer->GetBasePrecipitationRenderer()&&ShowRainTextures)
		{
			simulWeatherRenderer->GetBasePrecipitationRenderer()->RenderTextures(deviceContext,0,view->GetScreenHeight()/2,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(deviceContext,view->GetScreenWidth(),view->GetScreenHeight());
			const char *txt=Profiler::GetGlobalProfiler().GetDebugText();
			renderPlatformDx11.Print(deviceContext			,12	,12,txt);
		}
	}
	if(oceanRenderer&&ShowWaterTextures)
		oceanRenderer->RenderTextures(pContext,view->GetScreenWidth(),view->GetScreenHeight());
	SIMUL_COMBINED_PROFILE_END(pContext)
	SIMUL_COMBINED_PROFILE_END(pContext)
	Profiler::GetGlobalProfiler().EndFrame(pContext);
}

void Direct3D11Renderer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
	ID3D11DeviceContext* pContext=(ID3D11DeviceContext* )deviceContext.asD3D11DeviceContext();
	MixedResolutionView *view	=viewManager.GetView(deviceContext.viewStruct.view_id);
	view->RenderDepthBuffers(deviceContext,x0,y0,dx,dy);
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->RenderFramebufferDepth(deviceContext,x0+dx/2	,y0	,dx/2,dy/2);
		//UtilityRenderer::DrawTexture(pContext,2*w	,0,w,l,(ID3D11ShaderResourceView*)simulWeatherRenderer->GetFramebufferTexture(view_id)	,1.f		);
		//renderPlatformDx11.Print(pContext			,2.f*w	,0.f,"Near overlay");
		simulWeatherRenderer->RenderCompositingTextures(deviceContext,x0,y0+dy,dx,dy);
	}
}

void Direct3D11Renderer::SaveScreenshot(const char *filename_utf8)
{
	std::string screenshotFilenameUtf8=filename_utf8;
	MixedResolutionView *view=viewManager.GetView(0);
	if(!view)
		return;
	simul::dx11::Framebuffer fb(view->GetScreenWidth(),view->GetScreenHeight());
	fb.RestoreDeviceObjects(&renderPlatformDx11);
	crossplatform::DeviceContext deviceContext;
	ID3D11DeviceContext *pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	deviceContext.platform_context=pImmediateContext;
	fb.Activate(deviceContext);
	Render(0,m_pd3dDevice,pImmediateContext);
	fb.Deactivate(pImmediateContext);
	simul::dx11::SaveTexture(m_pd3dDevice,(ID3D11Texture2D *)(fb.GetColorTexture()),screenshotFilenameUtf8.c_str());
	SAFE_RELEASE(pImmediateContext);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	if(!m_pd3dDevice)
		return;
	std::cout<<"Direct3D11Renderer::OnD3D11LostDevice"<<std::endl;
	Profiler::GetGlobalProfiler().Uninitialize();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	renderPlatformDx11.InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	if(oceanRenderer)
		oceanRenderer->InvalidateDeviceObjects();
#ifdef SIMUL_USE_SCENE
	if(sceneRenderer)
		sceneRenderer->InvalidateDeviceObjects();
#endif
	viewManager.Clear();
	cubemapFramebuffer.InvalidateDeviceObjects();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	SAFE_RELEASE(lightProbesEffect);
	viewManager.InvalidateDeviceObjects();
	m_pd3dDevice=NULL;
}

void Direct3D11Renderer::OnD3D11DestroyDevice()
{
	OnD3D11LostDevice();
	// We don't clear the renderers because InvalidateDeviceObjects has already handled DX-specific destruction
	// And after OnD3D11DestroyDevice we might go back to startup without destroying the renderer.
}

bool Direct3D11Renderer::OnDeviceRemoved()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	OnD3D11LostDevice();
	return true;
}

void Direct3D11Renderer::RecompileShaders()
{
	simul::dx11::UtilityRenderer::RecompileShaders();
	cubemapFramebuffer.RecompileShaders();
	renderPlatformDx11.RecompileShaders();
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer)
		simulOpticsRenderer->RecompileShaders();
	if(simulTerrainRenderer)
		simulTerrainRenderer->RecompileShaders();
	if(oceanRenderer)
		oceanRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();

	std::map<std::string,std::string> defines;
	//["REVERSE_DEPTH"]		=ReverseDepth?"1":"0";
	//defines["NUM_AA_SAMPLES"]		=base::stringFormat("%d",Antialiasing);
	viewManager.RecompileShaders();
	SAFE_RELEASE(lightProbesEffect);
	if(m_pd3dDevice)
	{
		V_CHECK(dx11::CreateEffect(m_pd3dDevice,&lightProbesEffect,"light_probes.fx",defines));
		lightProbeConstants.LinkToEffect(lightProbesEffect,"LightProbeConstants");
	}
}

void    Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
{
}

const char *Direct3D11Renderer::GetDebugText() const
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

void Direct3D11Renderer::SetViewType(int view_id,ViewType vt)
{
	MixedResolutionView *v=viewManager.GetView(view_id);
	if(!v)
		return;
	v->viewType=vt;
}

void Direct3D11Renderer::SetCamera(int view_id,const simul::camera::CameraOutputInterface *c)
{
	cameras[view_id]=c;
}

void Direct3D11Renderer::ReverseDepthChanged()
{
	renderPlatformDx11.SetReverseDepth(ReverseDepth);
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulHDRRenderer)
		simulHDRRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
	if(oceanRenderer)
		oceanRenderer->SetReverseDepth(ReverseDepth);
	RecompileShaders();
}

void Direct3D11Renderer::AntialiasingChanged()
{
	RecompileShaders();
}