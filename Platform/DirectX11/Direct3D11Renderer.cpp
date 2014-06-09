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
using namespace simul;
using namespace dx11;

Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m,int w,int h):
		ShowCloudCrossSections(false)
		,ShowFlares(true)
		,Show2DCloudTextures(false)
		,ShowWaterTextures(false)
		,ShowFades(false)
		,ShowTerrain(true)
		,ShowMap(false)
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
#ifndef _XBOX_ONE
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
	
#ifndef _XBOX_ONE
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
#ifndef _XBOX_ONE
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

MixedResolutionRenderer::MixedResolutionRenderer()
		:mixedResolutionEffect(NULL)
		,m_pd3dDevice(NULL)
{
}
MixedResolutionRenderer::~MixedResolutionRenderer()
{
	InvalidateDeviceObjects();
}
void MixedResolutionRenderer::RestoreDeviceObjects(ID3D11Device* pd3dDevice)
{
	m_pd3dDevice=pd3dDevice;
	mixedResolutionConstants.RestoreDeviceObjects(pd3dDevice);
}

void MixedResolutionRenderer::InvalidateDeviceObjects()
{
	m_pd3dDevice=NULL;
	mixedResolutionConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(mixedResolutionEffect);
}

void MixedResolutionRenderer::RecompileShaders(const std::map<std::string,std::string> &defines)
{
	SAFE_RELEASE(mixedResolutionEffect);
	if(!m_pd3dDevice)
		return;
	HRESULT hr=CreateEffect(m_pd3dDevice,&mixedResolutionEffect,"mixed_resolution.fx",defines);
	mixedResolutionConstants.LinkToEffect(mixedResolutionEffect,"MixedResolutionConstants");
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
	mixedResolutionRenderer.RestoreDeviceObjects(pd3dDevice);
	//Set a global device pointer for use by various classes.
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	simul::dx11::UtilityRenderer::RestoreDeviceObjects(pd3dDevice);
	renderPlatformDx11.RestoreDeviceObjects(pd3dDevice);
	lightProbeConstants.RestoreDeviceObjects(m_pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(&renderPlatformDx11);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(&renderPlatformDx11);
	cubemapFramebuffer.SetWidthAndHeight(32,32);
	cubemapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	envmapFramebuffer.SetWidthAndHeight(64,64);
	envmapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	RecompileShaders();
}
int	Direct3D11Renderer::AddView				(bool external_fb)
{
	return viewManager.AddView(external_fb);
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
		view->RestoreDeviceObjects(m_pd3dDevice);
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
	D3DXMATRIX view;
	D3DXMATRIX proj;
	D3DXMATRIX view_matrices[6];
ERRNO_CHECK
	MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
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
	/*	crossplatform::DeviceContext deviceContext;
		deviceContext.platform_context	=pContext;
		deviceContext.renderPlatform	=&renderPlatformDx11;*/
		deviceContext.viewStruct.view_id=cubemap_view_id;
		deviceContext.viewStruct.view	=(const float*)&view_matrices[i];
		//cubemapDepthFramebuffer.Activate(pContext);
		//cubemapDepthFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
ERRNO_CHECK
		if(simulTerrainRenderer)
		{
			simulTerrainRenderer->Render(deviceContext,1.f);
		}
		cubemapFramebuffer.DeactivateDepth(deviceContext.platform_context);
ERRNO_CHECK
		if(simulWeatherRenderer)
		{
			simulWeatherRenderer->SetMatrices((const float*)&(view_matrices[i]),(const float*)&deviceContext.viewStruct.proj);
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
ERRNO_CHECK
			simulWeatherRenderer->RenderSkyAsOverlay(deviceContext
				,true,1.f,false,cubemapFramebuffer.GetDepthTexture(),NULL,relativeViewportTextureRegionXYWH,true);
ERRNO_CHECK
		}
		cubemapFramebuffer.Deactivate(deviceContext.platform_context);
	}
ERRNO_CHECK
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetCubemapTexture(envmapFramebuffer.GetColorTex());
ERRNO_CHECK
	if(oceanRenderer)
		oceanRenderer->SetCubemapTexture(cubemapFramebuffer.GetColorTex());
ERRNO_CHECK
}

void Direct3D11Renderer::RenderEnvmap(crossplatform::DeviceContext &deviceContext)
{
	cubemapFramebuffer.SetBands(SphericalHarmonicsBands);
	cubemapFramebuffer.CalcSphericalHarmonics(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	envmapFramebuffer.Clear(pContext,0.f,1.f,0.f,1.f,0.f);
	math::Matrix4x4 invViewProj;
	D3DXMATRIX view_matrices[6];
	float cam_pos[]={0,0,0};
	ID3DX11EffectTechnique *tech=lightProbesEffect->GetTechniqueByName("irradiance_map");
	MakeCubeMatrices(view_matrices,cam_pos,false);
	// For each face, 
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
}

void MixedResolutionRenderer::DownscaleDepth(crossplatform::DeviceContext &deviceContext,MixedResolutionView *view,int s,vec3 depthToLinFadeDistParams)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"DownscaleDepth")
	int W=0,H=0;
	ID3D11ShaderResourceView *depth_SRV=NULL;
	if(view->useExternalFramebuffer)
	{
		//D3D11_TEXTURE2D_DESC textureDesc;
		// recreate the SRV's if necessary:
		if(view->externalDepthTexture_SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
			view->externalDepthTexture_SRV->GetDesc(&depthDesc);
			H=view->ScreenHeight;
			W=view->ScreenWidth;
			depth_SRV=view->externalDepthTexture_SRV;
		}
	}
	else
	{
		W=view->GetFramebuffer()->Width;
		H=view->GetFramebuffer()->Height;
		depth_SRV=(ID3D11ShaderResourceView*)view->GetFramebuffer()->GetDepthTex();
	}
	if(!W||!H)
		return;
#if 1
	SIMUL_ASSERT(depth_SRV!=NULL);
	// The downscaled size should be enough to fit in at least s hi-res pixels in every larger pixel
	int w=(W+s-1)/s;
	int h=(H+s-1)/s;
	view->GetLowResDepthTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	view->GetLowResScratchTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	depth_SRV->GetDesc(&desc);
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups.
	static bool use_rt=true;
	{
		view->GetHiResDepthTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,W,H,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/!use_rt,/*rendertarget=*/use_rt);
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Hi-res Depth")
		mixedResolutionConstants.scale		=uint2(s,s);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ		=0;
		mixedResolutionConstants.farZ		=0;
		mixedResolutionConstants.source_dims=uint2(view->hiResDepthTexture.width,view->hiResDepthTexture.length);
		mixedResolutionConstants.Apply(deviceContext);
		static int BLOCKWIDTH			=8;
		uint2 subgrid						=uint2((view->hiResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(view->hiResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
		simul::dx11::setTexture				(mixedResolutionEffect,"sourceMSDepthTexture"	,depth_SRV);
		simul::dx11::setTexture				(mixedResolutionEffect,"sourceDepthTexture"		,depth_SRV);
		simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture"		,view->hiResDepthTexture.unorderedAccessView);
	
		if(use_rt)
		{
			view->GetHiResDepthTexture()->activateRenderTarget(deviceContext);
			simul::dx11::applyPass(pContext,mixedResolutionEffect,"make_depth_far_near",msaa?"msaa":"main");
			UtilityRenderer::DrawQuad(pContext);
			view->GetHiResDepthTexture()->deactivateRenderTarget();
		}
		else
		{
			simul::dx11::applyPass(pContext,mixedResolutionEffect,"cs_make_depth_far_near",msaa?"msaa":"main");
			pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		unbindTextures(mixedResolutionEffect);
		simul::dx11::applyPass(pContext,mixedResolutionEffect,"cs_make_depth_far_near",msaa?"msaa":"main");
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Lo-res Depth")
		mixedResolutionConstants.scale=uint2(s,s);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ=0;
		mixedResolutionConstants.farZ=0;
		mixedResolutionConstants.source_dims=uint2(view->GetHiResDepthTexture()->GetWidth(),view->GetHiResDepthTexture()->GetLength());
		// if using rendertarget we must rescale the texCoords.
		//mixedResolutionConstants.mixedResolutionTransformXYWH=vec4(0.f,0.f,(float)(w*s)/(float)W,(float)(h*s)/(float)H);
		mixedResolutionConstants.Apply(deviceContext);
		static int BLOCKWIDTH				=8;
		uint2 subgrid						=uint2((view->GetLowResDepthTexture()->GetWidth()+BLOCKWIDTH-1)/BLOCKWIDTH,(view->GetLowResDepthTexture()->GetLength()+BLOCKWIDTH-1)/BLOCKWIDTH);
		simul::dx11::setTexture				(mixedResolutionEffect,"sourceMSDepthTexture"	,depth_SRV);
		simul::dx11::setTexture				(mixedResolutionEffect,"sourceDepthTexture"		,(ID3D11ShaderResourceView *)view->GetHiResDepthTexture()->AsVoidPointer());
		simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture"		,((dx11::Texture *)view->GetLowResScratchTexture())->unorderedAccessView);
	
		simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near_from_hires");
		pContext->Dispatch(subgrid.x,subgrid.y,1);
		simul::dx11::setTexture				(mixedResolutionEffect,"sourceDepthTexture"		,(ID3D11ShaderResourceView *)view->GetLowResScratchTexture()->AsVoidPointer());
		simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture"		,((dx11::Texture *)view->GetLowResDepthTexture())->unorderedAccessView);
		simul::dx11::applyPass(pContext,mixedResolutionEffect,"spread_edge");
		pContext->Dispatch(subgrid.x,subgrid.y,1);
		unbindTextures(mixedResolutionEffect);
		simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near_from_hires");
		SIMUL_COMBINED_PROFILE_END(pContext)
			
		simul::dx11::setTexture(mixedResolutionEffect,"sourceMSDepthTexture",NULL);
		simul::dx11::setTexture(mixedResolutionEffect,"sourceDepthTexture"	,NULL);
		simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near_from_hires");
	}
#endif
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
	
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetMatrices((const float*)&deviceContext.viewStruct.view,(const float*)&deviceContext.viewStruct.proj);
	if(simulTerrainRenderer&&ShowTerrain)
	{
		math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view,false);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture(cam_pos));
		simulTerrainRenderer->Render(deviceContext,1.f);	
	}
#ifndef _XBOX_ONE
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
		void *depthTextureHiRes		=view->hiResDepthTexture.shaderResourceView;
	
		int s=simulWeatherRenderer->GetDownscale();
		mixedResolutionRenderer.DownscaleDepth(deviceContext,view,s,(const float *)simulWeatherRenderer->GetBaseSkyRenderer()->GetDepthToDistanceParameters((const float*)&deviceContext.viewStruct.proj));
		simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
		static bool test=true;
		const void* skyBufferDepthTex = (UseSkyBuffer&test)? view->lowResDepthTexture.shaderResourceView : depthTextureHiRes;
		//simulWeatherRenderer->RenderSkyAsOverlay(pContext,view_id,(const float*)v,(const float*)proj,false,Exposure,UseSkyBuffer
			//,depthTextureMS,skyBufferDepthTex
			//,relativeViewportTextureRegionXYWH
			//,true);
	#if 1
		simulWeatherRenderer->RenderMixedResolution(deviceContext,false,exposure,gamma,view->GetFramebuffer()->GetDepthTexture()
			,depthTextureHiRes,skyBufferDepthTex,relativeViewportTextureRegionXYWH);
	#endif
		if(simulHDRRenderer&&UseHdrPostprocessor)
			view->GetFramebuffer()->ActivateDepth(deviceContext);
		simulWeatherRenderer->RenderLightning(deviceContext,depthTextureHiRes,relativeViewportTextureRegionXYWH,simulWeatherRenderer->GetCloudDepthTexture(deviceContext.viewStruct.view_id));
		simulWeatherRenderer->DoOcclusionTests(deviceContext);
		simulWeatherRenderer->RenderPrecipitation(deviceContext,depthTextureHiRes,relativeViewportTextureRegionXYWH);
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
	D3DXMATRIX v,proj;
	MixedResolutionView *view=viewManager.GetView(view_id);
	EnsureCorrectBufferSizes(view_id);
	const camera::CameraOutputInterface *cam=cameras[view_id];
	SIMUL_ASSERT(cam!=NULL);
	const camera::CameraViewStruct &cameraViewStruct=cam->GetCameraViewStruct();
	if(cam)
	{
		float aspect=(float)view->GetScreenWidth()/(float)view->GetScreenHeight();
		if(ReverseDepth)
			proj=*((D3DXMATRIX*)cam->MakeDepthReversedProjectionMatrix(cameraViewStruct.nearZ,cameraViewStruct.InfiniteFarPlane?0.f:cameraViewStruct.farZ,aspect));
		else
			proj=*((D3DXMATRIX*)cam->MakeProjectionMatrix(cameraViewStruct.nearZ,cameraViewStruct.farZ,aspect));
		v=*((D3DXMATRIX*)(cam->MakeViewMatrix()));
		simul::dx11::UtilityRenderer::SetMatrices(v,proj);
	}
	
	crossplatform::DeviceContext deviceContext;
	
	deviceContext.platform_context	=pContext;
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=view_id;
	deviceContext.viewStruct.proj	=(const float*)&proj;
	deviceContext.viewStruct.view	=(const float*)&v;
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices((const float*)&v,(const float*)&proj);
		simulWeatherRenderer->PreRenderUpdate(deviceContext);
	}
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
			proj=*((D3DXMATRIX*)cam->MakeStereoProjectionMatrix(simul::camera::LEFT_EYE,cameraViewStruct.nearZ,cameraViewStruct.farZ,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth));
			v	=*((D3DXMATRIX*)cam->MakeStereoViewMatrix(simul::camera::LEFT_EYE));
			simul::dx11::UtilityRenderer::SetMatrices(v,proj);
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
			proj=*((D3DXMATRIX*)cam->MakeStereoProjectionMatrix(simul::camera::RIGHT_EYE,cameraViewStruct.nearZ,cameraViewStruct.farZ,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth));
			v	=*((D3DXMATRIX*)cam->MakeStereoViewMatrix(simul::camera::RIGHT_EYE));
			simul::dx11::UtilityRenderer::SetMatrices(v,proj);
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
		SIMUL_COMBINED_PROFILE_START(pContext,"Env+Cubemap")
		const float *cam_pos=simul::dx11::GetCameraPosVector((const float*)&v);
		RenderCubemap(deviceContext,cam_pos);
		RenderEnvmap(deviceContext);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
#if 1
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		view->GetFramebuffer()->SetAntialiasing(Antialiasing);
		view->GetFramebuffer()->Activate(deviceContext);
		SIMUL_COMBINED_PROFILE_START(pContext,"Clear")
		view->GetFramebuffer()->Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	else
	{
	//	view->GetFramebuffer()->ActivateDepth(pContext);
	//	view->GetFramebuffer()->ClearDepth(pContext,ReverseDepth?0.f:1.f);
	}
	RenderScene(deviceContext,simulWeatherRenderer,cameraViewStruct.exposure,cameraViewStruct.gamma);
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer.IsValid())
	{
		UtilityRenderer::DrawCubemap(pContext,(ID3D11ShaderResourceView*)cubemapFramebuffer.GetColorTex(),v,proj,-.7f,.7f);
		UtilityRenderer::DrawCubemap(pContext,(ID3D11ShaderResourceView*)envmapFramebuffer.GetColorTex(),v,proj,-.4f,.7f);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		view->GetFramebuffer()->Deactivate(pContext);
		view->ResolveFramebuffer(deviceContext);
		simulHDRRenderer->Render(deviceContext,view->GetResolvedHDRBuffer(),cameraViewStruct.exposure,cameraViewStruct.gamma);
	}
#endif
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
			simulWeatherRenderer->GetBasePrecipitationRenderer()->RenderTextures(pContext,0,view->GetScreenHeight()/2,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(deviceContext,view->GetScreenWidth(),view->GetScreenHeight());
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
		simulWeatherRenderer->RenderFramebufferDepth(deviceContext,x0+dx	,y0	,dx,dy);
		//UtilityRenderer::DrawTexture(pContext,2*w	,0,w,l,(ID3D11ShaderResourceView*)simulWeatherRenderer->GetFramebufferTexture(view_id)	,1.f		);
		//renderPlatformDx11.Print(pContext			,2.f*w	,0.f,"Near overlay");
		simulWeatherRenderer->RenderCompositingTextures(deviceContext,x0,y0+2*dx,dx,dy);
	}
}

void Direct3D11Renderer::SaveScreenshot(const char *filename_utf8)
{
	std::string screenshotFilenameUtf8=filename_utf8;
	//if(!views.size())
//		return;
	MixedResolutionView *view=viewManager.GetView(0);//(views.begin())->second;
	if(!view)
		return;
	simul::dx11::Framebuffer fb(view->GetScreenWidth(),view->GetScreenHeight());
	fb.RestoreDeviceObjects(m_pd3dDevice);
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
#ifndef _XBOX_ONE
	if(sceneRenderer)
		sceneRenderer->InvalidateDeviceObjects();
#endif
	viewManager.Clear();
	cubemapFramebuffer.InvalidateDeviceObjects();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	SAFE_RELEASE(lightProbesEffect);
	mixedResolutionRenderer.InvalidateDeviceObjects();
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
	defines["REVERSE_DEPTH"]		=ReverseDepth?"1":"0";
	defines["NUM_AA_SAMPLES"]		=base::stringFormat("%d",Antialiasing);
	mixedResolutionRenderer.RecompileShaders(defines);
	SAFE_RELEASE(lightProbesEffect);
	V_CHECK(CreateEffect(m_pd3dDevice,&lightProbesEffect,"light_probes.fx",defines,D3DCOMPILE_OPTIMIZATION_LEVEL3));
	lightProbeConstants.LinkToEffect(lightProbesEffect,"LightProbeConstants");
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
	MixedResolutionView *v=viewManager.GetView(view_id);
	if(!v)
		return;
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
}

void Direct3D11Renderer::AntialiasingChanged()
{
	RecompileShaders();
}