#include <tchar.h>
#include "Direct3D9Renderer.h"
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

Direct3D9Renderer::Direct3D9Renderer(simul::clouds::Environment *env,int w,int h)
	:simul::graph::meta::Group()
	,aspect(1.f)
	,simulWeatherRenderer(NULL)
	,simulHDRRenderer(NULL)
	,simulTerrainRenderer(NULL)
	,y_vertical(true)
	,ShowCloudCrossSections(false)
	,Show2DCloudTextures(false)
	,ShowLightVolume(false)
	,ShowFades(false)
	,CelestialDisplay(false)
	,ShowTerrain(true)
	,ShowMap(false)
	,show_osd(false)
	,time_mult(0.f)
	,ShowFlares(true)
	,device_reset(true)
	,UseHdrPostprocessor(true)
	,ShowWater(true)
	,ReverseDepth(true)
{
	simulWeatherRenderer=new SimulWeatherRenderer(env,true,w,h,true,true);
	if(simulWeatherRenderer)
		AddChild(simulWeatherRenderer.get());
	simulHDRRenderer=new SimulHDRRenderer(128,128);
	if(simulHDRRenderer&&simulWeatherRenderer)
		simulHDRRenderer->SetAtmospherics(simulWeatherRenderer->GetAtmosphericsRenderer());
	simulTerrainRenderer=NULL;//new SimulTerrainRenderer();
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		simulWeatherRenderer->GetSkyRenderer()->EnableMoon(true);
	SetYVertical(y_vertical);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetYVertical(y_vertical);
	simulOpticsRenderer=new SimulOpticsRendererDX9();
}

Direct3D9Renderer::~Direct3D9Renderer()
{
	OnDestroyDevice();
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

void Direct3D9Renderer::SetShowOSD(bool s)
{
	show_osd=s;
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

HRESULT Direct3D9Renderer::RestoreDeviceObjects(IDirect3DDevice9* pd3dDevice)
{
	RT::RestoreDeviceObjects(pd3dDevice);
	RT::SetScreenSize(width,height);
	float weather_restore_time=0.f,hdr_restore_time=0.f,terrain_restore_time=0.f,optics_restore_time=0.f;
	simul::base::Timer timer;

	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetScreenSize(width,height);
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	timer.UpdateTime();
	weather_restore_time=timer.Time/1000.f;
	if(simulHDRRenderer)
	{
		simulHDRRenderer->SetBufferSize(width,height);
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	}
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
	return S_OK;
}

void Direct3D9Renderer::SetYVertical(bool y)
{
	y_vertical=y;
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->SetYVertical(y);
	if(simulTerrainRenderer.get())
		simulTerrainRenderer->SetYVertical(y_vertical);
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
	if(simulWeatherRenderer)
		simulWeatherRenderer->Update(NULL,dt);
	
	if(simulTerrainRenderer)
	{
		if(simulWeatherRenderer)
		{
			if(simulWeatherRenderer->GetSkyRenderer())
				simulTerrainRenderer->SetSkyInterface(simulWeatherRenderer->GetSkyRenderer()->GetSkyKeyframer());
		}
		simulTerrainRenderer->Update(dt);
		if(simulWeatherRenderer)
		{
			if(simulWeatherRenderer->GetCloudRenderer())
			{
			//	simulTerrainRenderer->SetCloudTexture		(simulWeatherRenderer->GetCloudRenderer()->GetCloudTexture(),simulWeatherRenderer->GetCloudRenderer()->GetCloudInterface()->GetWrap());
				simulTerrainRenderer->SetCloudScales		(simulWeatherRenderer->GetCloudRenderer()->GetCloudScales());
				simulTerrainRenderer->SetCloudOffset		(simulWeatherRenderer->GetCloudRenderer()->GetCloudOffset());
				simulTerrainRenderer->setCloudInterpolation	(simulWeatherRenderer->GetEnvironment()->cloudKeyframer->GetInterpolation());
			}
			else
			{
				simulTerrainRenderer->SetCloudTextures		(NULL,false);
			}
		}
	}
	timer.FinishTime();
	simul::math::FirstOrderDecay(update_timing,timer.TimeSum,1.f,fTimeStep);
}

void Direct3D9Renderer::OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep)
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	fTime;fTimeStep;
	D3DXMATRIX world,view,proj;
	if(device_reset)
		RestoreDeviceObjects(pd3dDevice);
	if(camera)
	{
		view=camera->MakeViewMatrix(false);
		proj=camera->MakeProjectionMatrix(1.f,250000.f,aspect,y_vertical);
	}
	D3DXMatrixIdentity(&world);
    if(!SUCCEEDED(pd3dDevice->BeginScene()))
		return;
	static simul::base::Timer timer;
	timer.TimeSum=0.f;
	timer.StartTime();
	//pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF00FF00,1.0f,0L);
	//since our skybox will blend based on alpha we have to clear the backbuffer to this alpha value
	pd3dDevice->SetRenderState( D3DRS_ZENABLE,FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,FALSE);

	pd3dDevice->SetTransform(D3DTS_WORLD,&world);
	pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	// Make left-handed matrix if y is vertical
	pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		simulHDRRenderer->StartRender();
		simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
	}
	else
	{
		pd3dDevice->Clear(0L,NULL,D3DCLEAR_ZBUFFER|D3DCLEAR_TARGET,0xFF000000,1.0f,0L);
		simulWeatherRenderer->SetExposureHint(1.0f);
	}
	if(simulWeatherRenderer)
	{
#ifdef XBOX
		simulWeatherRenderer->SetMatrices(view,proj);
#endif
		simulWeatherRenderer->RenderSky(pd3dDevice,true,false);
	}
	if(simulWeatherRenderer&&simulWeatherRenderer->GetAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
		simulWeatherRenderer->GetAtmosphericsRenderer()->StartRender(NULL);
	if(simulTerrainRenderer&&ShowTerrain)
	{
		simulTerrainRenderer->SetMatrices(view,proj);
		simulTerrainRenderer->Render(NULL);
	}
	//if(simulHDRRenderer&&UseHdrPostprocessor)
	//	simulHDRRenderer->CopyDepthAlpha();
	if(simulWeatherRenderer&&simulWeatherRenderer->GetAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
		simulWeatherRenderer->GetAtmosphericsRenderer()->FinishRender(pd3dDevice);
	timer.UpdateTime();
	if(simulWeatherRenderer)
	{
		pd3dDevice->SetTransform(D3DTS_VIEW,&view);
		simulWeatherRenderer->RenderLateCloudLayer(pd3dDevice,true);
		simulWeatherRenderer->DoOcclusionTests();
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir(0,0,1.f,0),light(0,0,0,0);
			if(simulWeatherRenderer->GetSkyRenderer())
			{
				simul::sky::float4 dir,light,cam_pos;
				dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
			//CalcCameraPosition(cam_pos);
				GetCameraPosVector(view,y_vertical,cam_pos);
				light=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetLocalIrradiance(cam_pos.z/1000.f);
				simulOpticsRenderer->SetMatrices(view,proj);
				float exposure=1.f;
				if(simulHDRRenderer)
					exposure=simulHDRRenderer->GetExposure();
				simulOpticsRenderer->RenderFlare(NULL,
					exposure*(1.f-simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion())
					,dir,light);
			}
		}
		pd3dDevice->SetTransform(D3DTS_VIEW,&view);
		simulWeatherRenderer->RenderLightning(NULL);
		if(ShowLightVolume&&simulWeatherRenderer->GetCloudRenderer())
			simulWeatherRenderer->GetCloudRenderer()->RenderLightVolume();
		simulWeatherRenderer->RenderPrecipitation(NULL);
	}
	timer.UpdateTime();
	simul::math::FirstOrderDecay(weather_timing,timer.Time,1.f,fTimeStep);
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&ShowFades)
		simulWeatherRenderer->GetSkyRenderer()->RenderFades(pd3dDevice,width,height);

	if(simulHDRRenderer&&UseHdrPostprocessor)
		simulHDRRenderer->FinishRender(pd3dDevice);
	timer.UpdateTime();
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
		simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(NULL,width,height);
	simul::math::FirstOrderDecay(hdr_timing,timer.Time,1.f,fTimeStep);

	if(ShowCloudCrossSections&&simulWeatherRenderer&&simulWeatherRenderer->GetCloudRenderer())
	{
		simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(pd3dDevice,width,height);
	//	simulWeatherRenderer->GetCloudRenderer()->RenderDistances(width,height);
	}
	if(Show2DCloudTextures&&simulWeatherRenderer&&simulWeatherRenderer->Get2DCloudRenderer())
	{
	//	simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(width,height);
	}
	
	if(simulTerrainRenderer&&ShowMap)
		simulTerrainRenderer->RenderMap(width);
	pd3dDevice->EndScene();
	timer.FinishTime();
	simul::math::FirstOrderDecay(render_timing,timer.TimeSum,1.f,fTimeStep);
}


LRESULT Direct3D9Renderer::MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing)
{
	hWnd;uMsg;wParam;lParam;pbNoFurtherProcessing;
	return 0;
}

void Direct3D9Renderer::OnLostDevice()
{
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
	if(!show_osd)
		return (("DirectX 9"));
	tstring weather_text;
	if(!simulWeatherRenderer.get())
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

void Direct3D9Renderer::RecompileShaders()
{
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer.get())
		simulOpticsRenderer->RecompileShaders();
	if(simulHDRRenderer.get())
		simulHDRRenderer->RecompileShaders();
	if(simulTerrainRenderer.get())
		simulTerrainRenderer->RecompileShaders();
}