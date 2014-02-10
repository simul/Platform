#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
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
//#include "Simul/Platform/DirectX11/RenderPlatform.h"
//#include "Simul/Scene/BaseSceneRenderer.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/pi.h"
using namespace simul;
using namespace dx11;

 View::View()
	:camera(NULL)
	,viewType(MAIN_3D_VIEW)
	,ScreenWidth(0)
	,ScreenHeight(0)
 {
 }

 View::~View()
 {
	 InvalidateDeviceObjects();
 }

void View::RestoreDeviceObjects(ID3D11Device *pd3dDevice)
{
	hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
	resolvedDepth_fb.RestoreDeviceObjects(pd3dDevice);
	hdrFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	resolvedDepth_fb.SetFormat(DXGI_FORMAT_R32_FLOAT);
}

void View::InvalidateDeviceObjects()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	resolvedDepth_fb.InvalidateDeviceObjects();
	lowResDepthTexture.release();
}

int View::GetScreenWidth() const
{
	return ScreenWidth;
}
int View::GetScreenHeight() const
{
	return ScreenHeight;
}
void View::SetResolution(int w,int h)
{
	ScreenWidth=w;
	ScreenHeight=h;
}
//simul::dx11::RenderPlatform renderPlatform;

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
		,ShowDepthBuffers(false)
		,ShowLightVolume(false)
		,CelestialDisplay(false)
		,ShowWater(true)
		,MakeCubemap(true)
		,ShowCubemaps(false)
		,ShowRainTextures(false)
		,ReverseDepth(true)
		,ShowOSD(false)
		,Exposure(1.0f)
		,Antialiasing(1)
		,SphericalHarmonicsBands(4)
		,last_created_view_id(-1)
		,cubemap_view_id(-1)
		,enabled(false)
		,m_pd3dDevice(NULL)
		,mixedResolutionEffect(NULL)
		,lightProbesEffect(NULL)
		,simulOpticsRenderer(NULL)
		,simulWeatherRenderer(NULL)
		,simulHDRRenderer(NULL)
		,simulTerrainRenderer(NULL)
		,oceanRenderer(NULL)
	//	,sceneRenderer(NULL)
		,memoryInterface(m)
{
	simulHDRRenderer		=::new(memoryInterface) SimulHDRRendererDX1x(128,128);
	simulWeatherRenderer	=::new(memoryInterface) SimulWeatherRendererDX11(env,memoryInterface);
	simulOpticsRenderer		=::new(memoryInterface) SimulOpticsRendererDX1x(memoryInterface);
	simulTerrainRenderer	=::new(memoryInterface) TerrainRenderer(memoryInterface);
	simulTerrainRenderer->SetBaseSkyInterface(env->skyKeyframer);

	oceanRenderer			=new(memoryInterface) OceanRenderer(env->seaKeyframer);
	oceanRenderer->SetBaseSkyInterface(env->skyKeyframer);
	
//	sceneRenderer			=new(memoryInterface) scene::BaseSceneRenderer(sc,&renderPlatform);
	ReverseDepthChanged();
	cubemapFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	cubemapFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	
	envmapFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	envmapFramebuffer.SetDepthFormat(DXGI_FORMAT_UNKNOWN);
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	OnD3D11LostDevice();
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
	enabled=true;
	//Set a global device pointer for use by various classes.
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	simul::dx11::UtilityRenderer::RestoreDeviceObjects(pd3dDevice);
	mixedResolutionConstants.RestoreDeviceObjects(m_pd3dDevice);
	lightProbeConstants.RestoreDeviceObjects(m_pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(pd3dDevice);
	cubemapFramebuffer.SetWidthAndHeight(32,32);
	cubemapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	envmapFramebuffer.SetWidthAndHeight(64,64);
	envmapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	RecompileShaders();
}

int	Direct3D11Renderer::AddView()
{
	if(!enabled)
		return -1;
	last_created_view_id++;
	int view_id		=last_created_view_id;
	View *view		=views[view_id]=new View();
	return view_id;
}

void Direct3D11Renderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	View *view			=GetView(view_id);
	if(view)
	{	
		view->RestoreDeviceObjects(m_pd3dDevice);
		view->SetResolution(pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
	}
}

void Direct3D11Renderer::EnsureCorrectBufferSizes(int view_id)
{
	View *view			=GetView(view_id);
	if(!view)
		return;
	// Must have a whole number of full-res pixels per low-res pixel.
	int w=view->GetScreenWidth(),h=view->GetScreenHeight();
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetScreenSize(view_id,view->GetScreenWidth(),view->GetScreenHeight());
		int s					=simulWeatherRenderer->GetDownscale();
		w						=(view->GetScreenWidth() +s-1)/s;
		h						=(view->GetScreenHeight()+s-1)/s;
		view->SetResolution(w*s,h*s);
	}
	w=view->GetScreenWidth();
	if(view->viewType==OCULUS_VR)
		w=view->GetScreenWidth()/2;
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(w,view->GetScreenHeight());
	view->hdrFramebuffer	.SetWidthAndHeight(w,view->GetScreenHeight());
	view->resolvedDepth_fb	.SetWidthAndHeight(w,view->GetScreenHeight());
	view->hdrFramebuffer	.SetAntialiasing(Antialiasing);
}

void Direct3D11Renderer::RenderCubemap(ID3D11DeviceContext* pContext,D3DXVECTOR3 cam_pos)
{
	D3DXMATRIX view;
	D3DXMATRIX proj;
	D3DXMATRIX view_matrices[6];
	MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
	if(cubemap_view_id<0)
	{
		last_created_view_id++;
		cubemap_view_id=last_created_view_id;
	}
	cubemapFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	for(int i=0;i<6;i++)
	{
		cubemapFramebuffer.SetCurrentFace(i);
		cubemapFramebuffer.Activate(pContext);
		D3DXMATRIX cube_proj;
		float nearPlane=1.f;
		float farPlane=200000.f;
		if(ReverseDepth)
			cube_proj=simul::camera::Camera::MakeDepthReversedProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
		else
			cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane);
		//cubemapDepthFramebuffer.Activate(pContext);
		//cubemapDepthFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		if(simulTerrainRenderer)
		{
			simulTerrainRenderer->SetMatrices(view_matrices[i],cube_proj);
			simulTerrainRenderer->Render(pContext,1.f);
		}
		cubemapFramebuffer.DeactivateDepth(pContext);
		if(simulWeatherRenderer)
		{
			simulWeatherRenderer->SetMatrices((const float*)view_matrices[i],(const float*)cube_proj);
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
			simulWeatherRenderer->RenderSkyAsOverlay(pContext,cubemap_view_id,(const float*)view_matrices[i],(const float*)cube_proj,
				true,Exposure,false,cubemapFramebuffer.GetDepthTex(),NULL,relativeViewportTextureRegionXYWH,true);
		}
		cubemapFramebuffer.Deactivate(pContext);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetCubemapTexture(envmapFramebuffer.GetColorTex());
	if(oceanRenderer)
		oceanRenderer->SetCubemapTexture(cubemapFramebuffer.GetColorTex());
}	
	
void Direct3D11Renderer::RenderEnvmap(ID3D11DeviceContext* pContext)
{
	cubemapFramebuffer.SetBands(SphericalHarmonicsBands);
	cubemapFramebuffer.CalcSphericalHarmonics(pContext);
	envmapFramebuffer.Clear(pContext,0.f,1.f,0.f,1.f,0.f);
	D3DXMATRIX invViewProj;
	D3DXMATRIX view_matrices[6];
	float cam_pos[]={0,0,0};
	ID3DX11EffectTechnique *tech=lightProbesEffect->GetTechniqueByName("irradiance_map");
	MakeCubeMatrices(view_matrices,cam_pos,false);
	// For each face, 
	for(int i=0;i<6;i++)
	{
		envmapFramebuffer.SetCurrentFace(i);
		envmapFramebuffer.Activate(pContext);
		D3DXMATRIX cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,1.f,200000.f);
		{
			MakeInvViewProjMatrix(invViewProj,view_matrices[i],cube_proj);
			lightProbeConstants.invViewProj	=invViewProj;
			lightProbeConstants.numSHBands	=SphericalHarmonicsBands;
			lightProbeConstants.Apply(pContext);
			simul::dx11::setTexture(lightProbesEffect,"basisBuffer"	,cubemapFramebuffer.GetSphericalHarmonics().shaderResourceView);
			ApplyPass(pContext,tech->GetPassByIndex(0));
			UtilityRenderer::DrawQuad(pContext);
			simul::dx11::setTexture(lightProbesEffect,"basisBuffer"	,NULL);
			ApplyPass(pContext,tech->GetPassByIndex(0));
		}
		envmapFramebuffer.Deactivate(pContext);
	}
}

void Direct3D11Renderer::DownscaleDepth(int view_id,ID3D11DeviceContext* pContext,const D3DXMATRIX &proj)
{
	if(!simulWeatherRenderer)
		return;
	int s=simulWeatherRenderer->GetDownscale();
	View *view=GetView(view_id);
	if(!view)
		return;
	int w=view->hdrFramebuffer.Width/s;
	int h=view->GetScreenHeight()/s;
	// DXGI_FORMAT_D32_FLOAT->DXGI_FORMAT_R32_FLOAT
	view->lowResDepthTexture.ensureTexture2DSizeAndFormat(m_pd3dDevice,w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	//lowResDepthTexture_far.ensureTexture2DSizeAndFormat(m_pd3dDevice,w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	//lowResDepthTexture_scratch.ensureTexture2DSizeAndFormat(m_pd3dDevice,w,h,DXGI_FORMAT_R32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	ID3D11ShaderResourceView *depth_SRV=(ID3D11ShaderResourceView*)view->hdrFramebuffer.GetDepthTex();
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	depth_SRV->GetDesc(&desc);
	bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
	// Resolve depth first:
	ID3D11Texture2D *depthTexture=view->hdrFramebuffer.GetDepthTexture();
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups. So we use a framebufferinstead.
	view->resolvedDepth_fb.Activate(pContext);
		simul::dx11::setTexture(mixedResolutionEffect,"sourceMSDepthTexture"	,depth_SRV);
		simul::dx11::setTexture(mixedResolutionEffect,"sourceDepthTexture"		,depth_SRV);
		ID3DX11EffectTechnique *tech=mixedResolutionEffect->GetTechniqueByName("resolve_depth");
		V_CHECK(ApplyPass(pContext,tech->GetPassByIndex(0)));
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	view->resolvedDepth_fb.Deactivate(pContext);
	//pContext->ResolveSubresource(resolvedDepthTexture.texture, 0, depthTexture, 0, DXGI_FORMAT_R32_FLOAT);
	mixedResolutionConstants.scale=uint2(s,s);
	mixedResolutionConstants.depthToLinFadeDistParams=simulWeatherRenderer->GetBaseSkyRenderer()->GetDepthToDistanceParameters(proj);
	mixedResolutionConstants.nearZ=0;
	mixedResolutionConstants.farZ=0;
	mixedResolutionConstants.Apply(pContext);
	static const int BLOCKWIDTH			=8;
	uint2 subgrid						=uint2((view->lowResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(view->lowResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
	simul::dx11::setTexture				(mixedResolutionEffect,"sourceMSDepthTexture"	,depth_SRV);
	simul::dx11::setTexture				(mixedResolutionEffect,"sourceDepthTexture"		,depth_SRV);
	simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture"		,view->lowResDepthTexture.unorderedAccessView);
	
	simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near",msaa?"msaa":"main");
	pContext->Dispatch(subgrid.x,subgrid.y,1);
	unbindTextures(mixedResolutionEffect);
	simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near",msaa?"msaa":"main");
}

void Direct3D11Renderer::RenderFadeEditView(ID3D11DeviceContext* pContext)
{

}

void Direct3D11Renderer::RenderScene(int view_id,ID3D11DeviceContext* pContext,simul::clouds::BaseWeatherRenderer *w,D3DXMATRIX v,D3DXMATRIX proj)
{
	View *view=views[view_id];
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetMatrices((const float*)v,(const float*)proj);
	if(simulTerrainRenderer&&ShowTerrain)
	{
		simulTerrainRenderer->SetMatrices((const float*)v,(const float*)proj);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture());
		simulTerrainRenderer->Render(pContext,1.f);	
	}
	if(oceanRenderer&&ShowWater)
	{
		oceanRenderer->SetMatrices((const float*)v,(const float*)proj);
		oceanRenderer->Render(pContext,1.f);
		if(oceanRenderer->GetShowWireframes())
			oceanRenderer->RenderWireframe(pContext);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->RenderCelestialBackground(pContext,Exposure);
	if(simulHDRRenderer&&UseHdrPostprocessor)
		view->hdrFramebuffer.DeactivateDepth(pContext);
	else
		view->hdrFramebuffer.Deactivate(pContext);
	if(!simulWeatherRenderer)
		return;
	void *depthTextureMS		=view->hdrFramebuffer.GetDepthTex();
	void *depthTextureResolved	=view->resolvedDepth_fb.GetColorTex();
	DownscaleDepth(view_id,pContext,proj);
	simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
	static bool test=true;
	const void* skyBufferDepthTex = (UseSkyBuffer&test)? view->lowResDepthTexture.shaderResourceView : depthTextureResolved;
	//simulWeatherRenderer->RenderSkyAsOverlay(pContext,view_id,(const float*)v,(const float*)proj,false,Exposure,UseSkyBuffer
		//,depthTextureMS,skyBufferDepthTex
		//,relativeViewportTextureRegionXYWH
		//,true);
	simulWeatherRenderer->RenderMixedResolution(pContext,view_id,(const float*)v,(const float*)proj,false,Exposure,depthTextureMS,skyBufferDepthTex,relativeViewportTextureRegionXYWH);
	simulWeatherRenderer->RenderLightning(pContext,view_id);
	simulWeatherRenderer->DoOcclusionTests();
	simulWeatherRenderer->RenderPrecipitation(pContext,depthTextureResolved,relativeViewportTextureRegionXYWH);
	if(!simulOpticsRenderer||!ShowFlares)
		return;
	simul::sky::float4 dir,light;
	if(!simulWeatherRenderer->GetSkyRenderer())
		return;
	dir			=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
	light		=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
	simulOpticsRenderer->SetMatrices(v,proj);
	float occ	=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
	float exp	=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
	simulOpticsRenderer->RenderFlare(pContext,exp,dir,light);
}

void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	if(!enabled)
		return;
	simul::base::SetGpuProfilingInterface(pContext,&simul::dx11::Profiler::GetGlobalProfiler());
	D3DXMATRIX v,proj;
	static float nearPlane=1.f;
	static float farPlane=250000.f;
	View *view=views[view_id];
	if(view->viewType==FADE_EDITING)
	{
		RenderFadeEditView(pContext);
		return;
	}
	EnsureCorrectBufferSizes(view_id);
	const camera::CameraOutputInterface *cam=view->camera;
	if(cam)
	{
		float aspect=(float)view->GetScreenWidth()/(float)view->GetScreenHeight();
		if(ReverseDepth)
			proj=cam->MakeDepthReversedProjectionMatrix(nearPlane,farPlane,aspect);
		else
			proj=cam->MakeProjectionMatrix(nearPlane,farPlane,aspect);
		v=cam->MakeViewMatrix();
		simul::dx11::UtilityRenderer::SetMatrices(v,proj);
	}
	else
		SIMUL_ASSERT(false);
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices((const float*)v,(const float*)proj);
		simulWeatherRenderer->PreRenderUpdate(pContext);
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
			proj=cam->MakeStereoProjectionMatrix(simul::camera::LEFT_EYE,nearPlane,farPlane,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
			v	=cam->MakeStereoViewMatrix(simul::camera::LEFT_EYE);
			simul::dx11::UtilityRenderer::SetMatrices(v,proj);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->hdrFramebuffer.SetAntialiasing(Antialiasing);
			view->hdrFramebuffer.Activate(pContext);
			view->hdrFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		}
		RenderScene(view_id,pContext,simulWeatherRenderer,v,proj);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->hdrFramebuffer.Deactivate(pContext);
			simulHDRRenderer->RenderWithOculusCorrection(pContext,view->hdrFramebuffer.GetColorTex(),0.f);
		}
		//right eye
		if(cam)
		{
			proj=cam->MakeStereoProjectionMatrix(simul::camera::RIGHT_EYE,nearPlane,farPlane,(float)view->GetScreenWidth()/2.f/(float)view->GetScreenHeight(),ReverseDepth);
			v	=cam->MakeStereoViewMatrix(simul::camera::RIGHT_EYE);
			simul::dx11::UtilityRenderer::SetMatrices(v,proj);
		}
		viewport.TopLeftX			=view->GetScreenWidth()/2.f;
		pContext->RSSetViewports(1, &viewport);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->hdrFramebuffer.SetAntialiasing(Antialiasing);
			view->hdrFramebuffer.Activate(pContext);
			view->hdrFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		}
		RenderScene(view_id,pContext,simulWeatherRenderer,v,proj);
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			view->hdrFramebuffer.Deactivate(pContext);
			simulHDRRenderer->RenderWithOculusCorrection(pContext,view->hdrFramebuffer.GetColorTex(),1.f);
		}
		return;
	}
	if(MakeCubemap)
	{
		D3DXVECTOR3 cam_pos=simul::dx11::GetCameraPosVector(v);
		RenderCubemap(pContext,cam_pos);
		RenderEnvmap(pContext);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		view->hdrFramebuffer.SetAntialiasing(Antialiasing);
		view->hdrFramebuffer.Activate(pContext);
		view->hdrFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	}
	else
	{
	//	view->hdrFramebuffer.ActivateDepth(pContext);
	//	view->hdrFramebuffer.ClearDepth(pContext,ReverseDepth?0.f:1.f);
	}
	RenderScene(view_id,pContext,simulWeatherRenderer,v,proj);
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer.IsValid())
	{
		UtilityRenderer::DrawCubemap(pContext,(ID3D1xShaderResourceView*)cubemapFramebuffer.GetColorTex(),v,proj,-.7f,.7f);
		UtilityRenderer::DrawCubemap(pContext,(ID3D1xShaderResourceView*)envmapFramebuffer.GetColorTex(),v,proj,-.4f,.7f);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		view->hdrFramebuffer.Deactivate(pContext);
		simulHDRRenderer->Render(pContext,view->hdrFramebuffer.GetColorTex());
	}
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(pContext,view->GetScreenWidth(),view->GetScreenHeight());
		simul::dx11::UtilityRenderer::SetScreenSize(view->GetScreenWidth(),view->GetScreenHeight());
		bool vertical_screen=(view->GetScreenHeight()>view->GetScreenWidth());
		if(ShowFades&&simulWeatherRenderer->GetSkyRenderer())
		{
			int x0=view->GetScreenWidth()/2;
			int y0=8;
			if(vertical_screen)
			{
				x0=8;
				y0=view->GetScreenHeight()/2;
			}
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(pContext,x0,y0,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
		}
		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer())
		{
			int x0=8;
			int y0=8;
			simulWeatherRenderer->RenderFramebufferDepth(pContext,0,view->GetScreenWidth(),view->GetScreenHeight());
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(pContext,x0,y0,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(pContext,x0,y0,view->GetScreenWidth()/2,view->GetScreenHeight()/2);
		}
		if(ShowDepthBuffers)
		{
			int w=view->lowResDepthTexture.width*4;
			int l=view->lowResDepthTexture.length*4;
			if(w>view->hdrFramebuffer.Width/4)
			{
				l*=view->hdrFramebuffer.Width/4;
				l/=w;
				w=view->hdrFramebuffer.Width/4;
			}
			if(l>view->hdrFramebuffer.Height/4)
			{
				w*=view->hdrFramebuffer.Height/4;
				w/=l;
				l=view->hdrFramebuffer.Height/4;
			}
			UtilityRenderer::DrawTexture(pContext	,0*w	,0,w,l,(ID3D1xShaderResourceView*)view->hdrFramebuffer.GetDepthTex()				,10000.0f	);
			UtilityRenderer::Print(pContext			,0.f*w	,0.f,"Main Depth");
			//UtilityRenderer::DrawTexture(pContext,1*w	,0,w,l,(ID3D1xShaderResourceView*)view->resolvedDepth_fb.GetColorTex()					,10000.0f	);
			UtilityRenderer::DrawTexture(pContext	,1*w	,0,w,l,view->lowResDepthTexture.shaderResourceView									,10000.0f	);
			UtilityRenderer::Print(pContext			,1.f*w	,0.f,"Lo-Res Depth");
			//UtilityRenderer::DrawTexture(pContext,2*w	,0,w,l,(ID3D1xShaderResourceView*)simulWeatherRenderer->GetFramebufferTexture(view_id)	,1.f		);
			//UtilityRenderer::Print(pContext			,2.f*w	,0.f,"Near overlay");
			simulWeatherRenderer->RenderCompositingTextures(pContext,0,view->hdrFramebuffer.Width,view->hdrFramebuffer.Height);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(pContext,0,0,view->GetScreenWidth(),view->GetScreenHeight());
		}
		if(simulWeatherRenderer->GetBasePrecipitationRenderer()&&ShowRainTextures)
		{
			simulWeatherRenderer->GetBasePrecipitationRenderer()->RenderTextures(pContext,view->GetScreenWidth(),view->GetScreenHeight());
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(pContext,view->GetScreenWidth(),view->GetScreenHeight());
		}
	}
	if(oceanRenderer&&ShowWaterTextures)
		oceanRenderer->RenderTextures(pContext,view->GetScreenWidth(),view->GetScreenHeight());
	Profiler::GetGlobalProfiler().EndFrame(pContext);
}

void Direct3D11Renderer::SaveScreenshot(const char *filename_utf8)
{
	std::string screenshotFilenameUtf8=filename_utf8;
	View *view=views[0];
	simul::dx11::Framebuffer fb(view->GetScreenWidth(),view->GetScreenHeight());
	fb.RestoreDeviceObjects(m_pd3dDevice);
	ID3D11DeviceContext *pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	fb.Activate(pImmediateContext);
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
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
	{
		i->second->hdrFramebuffer.InvalidateDeviceObjects();
		i->second->resolvedDepth_fb.InvalidateDeviceObjects();
		i->second->lowResDepthTexture.release();
	}
	cubemapFramebuffer.InvalidateDeviceObjects();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	SAFE_RELEASE(mixedResolutionEffect);
	mixedResolutionConstants.InvalidateDeviceObjects();
	m_pd3dDevice=NULL;
}

void Direct3D11Renderer::OnD3D11DestroyDevice()
{
	OnD3D11LostDevice();
	// We don't clear the renderers because InvalidateDeviceObjects has already handled DX-specific destruction
	// And after OnD3D11DestroyDevice we might go back to startup without destroying the renderer.
}

void Direct3D11Renderer::RemoveView(int view_id)
{
	delete views[view_id];
	views.erase(view_id);
    //Profiler::GetGlobalProfiler().Uninitialize();
	//if(simulWeatherRenderer)
	//	simulWeatherRenderer->InvalidateDeviceObjects();
	//if(simulHDRRenderer)
	//	simulHDRRenderer->InvalidateDeviceObjects();
	//OnD3D11LostDevice();
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
//	if(simulTerrainRenderer.get())
//		simulTerrainRenderer->RecompileShaders();
	SAFE_RELEASE(mixedResolutionEffect);
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	HRESULT hr=CreateEffect(m_pd3dDevice,&mixedResolutionEffect,"mixed_resolution.fx",defines);
	mixedResolutionConstants.LinkToEffect(mixedResolutionEffect,"MixedResolutionConstants");
	SAFE_RELEASE(lightProbesEffect);
	V_CHECK(CreateEffect(m_pd3dDevice,&lightProbesEffect,"light_probes.fx"));
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
	View *v=GetView(view_id);
	if(!v)
		return;
	v->viewType=vt;
}

void Direct3D11Renderer::SetCamera(int view_id,const simul::camera::CameraOutputInterface *c)
{
	View *v=GetView(view_id);
	if(!v)
		return;
	v->camera=c;
}

void Direct3D11Renderer::ReverseDepthChanged()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulHDRRenderer)
		simulHDRRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
	if(oceanRenderer)
		oceanRenderer->SetReverseDepth(ReverseDepth);
}

View *Direct3D11Renderer::GetView(int view_id)
{
	ViewMap::iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}