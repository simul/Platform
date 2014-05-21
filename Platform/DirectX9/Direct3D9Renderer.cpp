#define NOMINMAX
#include <tchar.h>
#include "Direct3D9Renderer.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
#include "Simul/Platform/DirectX9/SimulWeatherRenderer.h"
#include "Simul/Platform/DirectX9/SimulCloudRenderer.h"
#include "Simul/Platform/DirectX9/Simul2DCloudRenderer.h"
#include "Simul/Platform/DirectX9/SimulHDRRenderer.h"
#include "Simul/Platform/DirectX9/SimulAtmosphericsRenderer.h"
#include "Simul/Platform/DirectX9/SimulSkyRenderer.h"
#include "Simul/Platform/DirectX9/SimulTerrainRenderer.h"
#include "Simul/Platform/DirectX9/SimulOpticsRendererDX9.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/Resources.h"
#include <iomanip>

using namespace simul;
using namespace dx9;

Direct3D9Renderer::Direct3D9Renderer(simul::clouds::Environment *env,int w,int h)
	:simul::graph::meta::Group()
	,aspect(1.f)
	,simulWeatherRenderer(NULL)
	,simulHDRRenderer(NULL)
	,simulTerrainRenderer(NULL)
	,simulOpticsRenderer(NULL)
	,ShowCloudCrossSections(false)
	,Show2DCloudTextures(false)
	,ShowLightVolume(false)
	,ShowFades(false)
	,CelestialDisplay(false)
	,ShowTerrain(true)
	,ShowMap(false)
	,ShowOSD(false)
	,time_mult(0.f)
	,ShowFlares(true)
	,device_reset(true)
	,UseHdrPostprocessor(true)
	,ShowWater(true)
	,ReverseDepth(true)
{
	simulWeatherRenderer=new SimulWeatherRenderer(env,NULL,true,w,h,true,true);

	simulHDRRenderer=new SimulHDRRenderer(128,128);
	hdrFramebuffer.SetWidthAndHeight(w,h);
	hdrFramebuffer.SetFormat(D3DFMT_A32B32G32R32F);
	D3DFORMAT INTZ=((D3DFORMAT) MAKEFOURCC('I','N','T','Z'));
	hdrFramebuffer.SetDepthFormat(INTZ);//D3DFMT_D24X8);			//! This is one of only three formats that can be used as textures.
	if(simulHDRRenderer&&simulWeatherRenderer)
		simulHDRRenderer->SetAtmospherics(simulWeatherRenderer->GetAtmosphericsRenderer());
/*	simulTerrainRenderer=new SimulTerrainRenderer(NULL);
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		simulWeatherRenderer->GetSkyRenderer()->EnableMoon(true);*/
	simulOpticsRenderer=new SimulOpticsRendererDX9(NULL);
}

Direct3D9Renderer::~Direct3D9Renderer()
{
	OnLostDevice();
	delete simulWeatherRenderer;
	delete simulHDRRenderer;
	delete simulTerrainRenderer;
	delete simulOpticsRenderer;
}

bool Direct3D9Renderer::IsDeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat, bool bWindowed)
{
	pCaps;AdapterFormat;BackBufferFormat;bWindowed;
	return true;
}

bool Direct3D9Renderer::ModifyDeviceSettings(struct DXUTDeviceSettings* pDeviceSettings)
{
	pDeviceSettings;
	return true;
}
HRESULT Direct3D9Renderer::OnCreateDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc)
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->Create(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->Create(pd3dDevice);
	if(simulWeatherRenderer&&simulHDRRenderer)
		simulHDRRenderer->SetAtmospherics(simulWeatherRenderer->GetAtmosphericsRenderer());

	width=pBackBufferSurfaceDesc->Width;
	height=pBackBufferSurfaceDesc->Height;
	aspect=width/(float)height;
	return S_OK;
}

void Direct3D9Renderer::SetCamera(simul::camera::Camera *c)
{
	camera=c;
}

HRESULT Direct3D9Renderer::OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc)
{
	width=pBackBufferSurfaceDesc->Width;
	height=pBackBufferSurfaceDesc->Height;
	aspect=width/(float)height;
	device_reset=true;
	if(device_reset)
		RestoreDeviceObjects(pd3dDevice);
	return S_OK;
}

void Direct3D9Renderer::ReverseDepthChanged()
{
}

void Direct3D9Renderer::RestoreDeviceObjects(IDirect3DDevice9* pd3dDevice)
{
	hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
	RT::RestoreDeviceObjects(pd3dDevice);
	RT::SetScreenSize(width,height);
	float weather_restore_time=0.f,hdr_restore_time=0.f,terrain_restore_time=0.f,optics_restore_time=0.f;
	simul::base::Timer timer;

	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetScreenSize(0,width,height);
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	timer.UpdateTime();
	weather_restore_time=timer.Time/1000.f;
	if(simulHDRRenderer)
	{
		simulHDRRenderer->SetBufferSize(width,height);
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	hdrFramebuffer.SetWidthAndHeight(width,height);
	hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
	timer.UpdateTime();
	hdr_restore_time=timer.Time/1000.f;
	if(simulTerrainRenderer)
	{
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		{
			simulTerrainRenderer->SetSkyInterface(simulWeatherRenderer->GetSkyRenderer()->GetSkyKeyframer());
			simulTerrainRenderer->SetMaxFadeDistanceKm(simulWeatherRenderer->GetSkyRenderer()->GetSkyKeyframer()->GetMaxDistanceKm());
		}
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	timer.UpdateTime();
	terrain_restore_time=timer.Time/1000.f;
	if(simulOpticsRenderer)
	{
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	timer.FinishTime();
	optics_restore_time=timer.Time/1000.f;

	std::cout<<std::setprecision(4)<<"RESTORE TIMINGS: weather="<<weather_restore_time
		<<", hdr="<<hdr_restore_time<<", terrain="<<terrain_restore_time<<", optics="<<optics_restore_time<<std::endl;
	device_reset=false;
}
static float render_timing=0,update_timing=0,weather_timing=0,hdr_timing=0;
void Direct3D9Renderer::OnFrameMove(double fTime, float fTimeStep)
{
	static simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();
	fTime;
	framerate*=0.99f;
	float new_framerate=0.f;
	if(fTimeStep)
		new_framerate=1.f/fTimeStep;
	framerate+=0.01f*(new_framerate);

	// Using daytime? or regular time?
	float dt=time_mult*fTimeStep;
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyKeyframer()&&simulWeatherRenderer->GetSkyKeyframer()->GetLinkKeyframeTimeAndDaytime())
		dt/=(24.f*60.f*60.f);
	static float constant_timestep=0.f;
	if(constant_timestep)
		dt=constant_timestep;
	timer.FinishTime();
	simul::math::FirstOrderDecay(update_timing,timer.TimeSum,1.f,fTimeStep);
}

void Direct3D9Renderer::EnsureCorrectBufferSizes(int view_id)
{
	// Must have a whole number of full-res pixels per low-res pixel.
	int w=width,h=height;
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetScreenSize(view_id,width,height);
		int s					=simulWeatherRenderer->GetDownscale();
		w						=(w +s-1)/s;
		h						=(h+s-1)/s;
	}
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(width,height);
	hdrFramebuffer	.SetWidthAndHeight(width,height);
}

void Direct3D9Renderer::OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep)
{
	EnsureCorrectBufferSizes(0);
	static float exposure=1.f;
	static int viewport_id=0;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	fTime;fTimeStep;
	simul::crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=pd3dDevice;
	deviceContext.viewStruct.view_id=0;
	D3DXMATRIX world;
	if(device_reset)
		RestoreDeviceObjects(pd3dDevice);
	if(camera)
	{
		deviceContext.viewStruct.view=camera->MakeViewMatrix();
		deviceContext.viewStruct.proj=camera->MakeProjectionMatrix(1.f,250000.f,aspect);
	}
	D3DXMatrixIdentity(&world);
    if(!SUCCEEDED(pd3dDevice->BeginScene()))
		return;
#if 1
	if(simulWeatherRenderer)
		simulWeatherRenderer->PreRenderUpdate(NULL,fTimeStep);
	//since our skybox will blend based on alpha we have to clear the backbuffer to this alpha value
	pd3dDevice->SetRenderState( D3DRS_ZENABLE,FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,FALSE);
	pd3dDevice->SetTransform(D3DTS_WORLD,&world);
	pd3dDevice->SetTransform(D3DTS_VIEW,(D3DXMATRIX*)((const float *)deviceContext.viewStruct.view));
	// Make left-handed matrix if y is vertical
	pd3dDevice->SetTransform(D3DTS_PROJECTION,(D3DXMATRIX*)((const float *)deviceContext.viewStruct.proj));
	
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		hdrFramebuffer.Activate(NULL);
		static float depth_start=1.f;
		pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
	}
	else
	{
		pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,1.0f,0L);
	}
/*	if(simulTerrainRenderer&&ShowTerrain)
	{
		simulTerrainRenderer->SetMatrices(view,proj);
		simulTerrainRenderer->Render(NULL,1.f);
	}*/
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		// We would LIKE to use the depth buffer as a texture from here onwards, but this is not well-supported.
		hdrFramebuffer.DeactivateDepth(NULL);
	}
	void *depth_texture=hdrFramebuffer.GetDepthTex();
	if(simulWeatherRenderer)
	{
		pd3dDevice->SetTransform(D3DTS_VIEW,(D3DXMATRIX*)((const float *)deviceContext.viewStruct.view));
		simulWeatherRenderer->RenderSkyAsOverlay(deviceContext,false,exposure,UseSkyBuffer,depth_texture,NULL,simul::sky::float4(0,0,1.f,1.f),true);

	/*	simulWeatherRenderer->DoOcclusionTests(pd3dDevice);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir(0,0,1.f,0),light(0,0,0,0);
			if(simulWeatherRenderer->GetSkyRenderer())
			{
				simul::sky::float4 dir,light,cam_pos;
				dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			
				GetCameraPosVector(view,false,cam_pos);
				light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
				simulOpticsRenderer->SetMatrices(view,proj);
				float exposure=1.f;
				if(simulHDRRenderer)
					exposure=simulHDRRenderer->GetExposure();
				simulOpticsRenderer->RenderFlare(NULL,
					exposure*(1.f-simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion())
					,dir,light);
			}
		}*/
		pd3dDevice->SetTransform(D3DTS_VIEW,(D3DXMATRIX*)((const float *)deviceContext.viewStruct.view));
		simulWeatherRenderer->RenderLightning(NULL,viewport_id);
		simulWeatherRenderer->RenderPrecipitation(NULL);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		hdrFramebuffer.Deactivate(pd3dDevice);
		simulHDRRenderer->Render(pd3dDevice,hdrFramebuffer.GetColorTex());
	}
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&ShowFades)
		simulWeatherRenderer->GetSkyRenderer()->RenderFades(deviceContext,0,0,width,height);
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(NULL,width,height);
		if(ShowLightVolume&&simulWeatherRenderer->GetCloudRenderer())
			simulWeatherRenderer->GetCloudRenderer()->RenderLightVolume();
		if(ShowCloudCrossSections&&simulWeatherRenderer&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(deviceContext,0,0,width,height);
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(pd3dDevice,width,height);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(deviceContext,0,0,width,height);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer&&simulWeatherRenderer->Get2DCloudRenderer())
		{
		//	simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(width,height);
		}
	}
	
	if(simulTerrainRenderer&&ShowMap)
		simulTerrainRenderer->RenderMap(width);
#endif
	pd3dDevice->EndScene();
}


LRESULT Direct3D9Renderer::MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing)
{
	hWnd;uMsg;wParam;lParam;pbNoFurtherProcessing;
	return 0;
}

void Direct3D9Renderer::OnLostDevice()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	RT::InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
}

void Direct3D9Renderer::OnDestroyDevice()
{
	OnLostDevice();
}

const char *Direct3D9Renderer::GetDebugText() const
{
	static char debug_text[512];
	if(!ShowOSD)
		return (("DirectX 9"));
	tstring weather_text;
	if(!simulWeatherRenderer)
		return (("DirectX 9"));
#ifdef _UNICODE
	weather_text=simul::base::StringToWString(simulWeatherRenderer->GetDebugText());
#else
	weather_text=simulWeatherRenderer->GetDebugText();
#endif
	if(simulWeatherRenderer)
		sprintf_s(debug_text,256,("DirectX 9\n%s\nFramerate %3.3g Render time %3.3g weather %3.3g hdr %3.3g\nUpdate time %3.3g"),weather_text.c_str(),framerate,render_timing,weather_timing,hdr_timing,update_timing);
	return debug_text;
}
#include "SaveTexture.h"
void Direct3D9Renderer::SaveScreenshot(const char *filename_utf8)
{
	SaveTexture((LPDIRECT3DTEXTURE9)(hdrFramebuffer.GetColorTex()),filename_utf8);
}

void Direct3D9Renderer::RecompileShaders()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer)
		simulOpticsRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
	if(simulTerrainRenderer)
		simulTerrainRenderer->RecompileShaders();
}