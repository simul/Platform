#include <tchar.h>
#include "Direct3D9Renderer.h"
#include "Simul/Platform/DirectX 9/SimulWeatherRenderer.h"
#include "Simul/Platform/DirectX 9/SimulCloudRenderer.h"
#include "Simul/Platform/DirectX 9/Simul2DCloudRenderer.h"
#include "Simul/Platform/DirectX 9/SimulHDRRenderer.h"
#include "Simul/Platform/DirectX 9/SimulAtmosphericsRenderer.h"
#include "Simul/Platform/DirectX 9/SimulSkyRenderer.h"
#include "Simul/Platform/DirectX 9/SimulTerrainRenderer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Graph/Camera/Camera.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/DirectX 9/Macros.h"
#include "Simul/Platform/DirectX 9/Resources.h"

//extern unsigned GetResourceIdImplementation(const char *filename);
extern LPDIRECT3DVERTEXDECLARATION9	m_pHudVertexDecl;

Direct3D9Renderer::Direct3D9Renderer()
	:simul::graph::meta::Group()
	,Gamma(0.45f)
	,aspect(1.f)
	,Exposure(1.f)
	,simulWeatherRenderer(NULL)
	,simulHDRRenderer(NULL)
	,simulTerrainRenderer(NULL)
	,y_vertical(true)
	,show_cloud_sections(true)
	,render_light_volume(false)
	,show_fades(true)
	,celestial_display(false)
	,show_terrain(true)
	,show_map(false)
	,show_osd(false)
	,time_mult(0.f)
{
	//GetResourceId=&GetResourceIdImplementation;
	simulWeatherRenderer=new SimulWeatherRenderer(true,640,360,true,true,false,true,false);
	AddChild(simulWeatherRenderer.get());
	simulHDRRenderer=new SimulHDRRenderer(128,128);
	if(simulHDRRenderer)
		simulHDRRenderer->SetAtmospherics(simulWeatherRenderer->GetAtmosphericsRenderer());
	simulTerrainRenderer=new SimulTerrainRenderer();
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		simulWeatherRenderer->GetSkyRenderer()->EnableMoon(true);
	SetYVertical(y_vertical);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetYVertical(y_vertical);
}

Direct3D9Renderer::~Direct3D9Renderer()
{
}

bool Direct3D9Renderer::IsDeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat, bool bWindowed)
{
	pCaps;AdapterFormat;BackBufferFormat;bWindowed;
	return true;
}

bool Direct3D9Renderer::ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings)
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
	if(simulWeatherRenderer)
	if(simulWeatherRenderer->GetSkyRenderer())
	{
		simul::graph::meta::Node *n=dynamic_cast<simul::graph::meta::Node *>(simulWeatherRenderer->GetSkyRenderer()->GetSkyInterface());
		AddChild(n);
	}
	aspect=pBackBufferSurfaceDesc->Width/(float)pBackBufferSurfaceDesc->Height;
	width=pBackBufferSurfaceDesc->Width;
	height=pBackBufferSurfaceDesc->Height;
	return S_OK;
}

void Direct3D9Renderer::SetShowOSD(bool s)
{
	show_osd=s;
}
void Direct3D9Renderer::SetCamera(simul::graph::camera::Camera *c)
{
	camera=c;
}

HRESULT Direct3D9Renderer::OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc)
{
	aspect=pBackBufferSurfaceDesc->Width/(FLOAT)pBackBufferSurfaceDesc->Height;
	width=pBackBufferSurfaceDesc->Width;
	height=pBackBufferSurfaceDesc->Height;
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetBufferSize(width,height);
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	if(simulHDRRenderer)
	{
		simulHDRRenderer->SetBufferSize(pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	if(simulTerrainRenderer)
	{
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
		{
			simulTerrainRenderer->SetSkyInterface(simulWeatherRenderer->GetSkyRenderer()->GetSkyKeyframer());
			simulTerrainRenderer->SetMaxFadeDistanceKm(simulWeatherRenderer->GetSkyRenderer()->GetMaxFadeDistanceKm());
		}
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	}
	return S_OK;
}

void Direct3D9Renderer::SetTimeMultiplier(float fTimeMult)
{
	time_mult=fTimeMult;
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

	// Using daytime:
	float dt=time_mult*fTimeStep/(24.f*60.f*60.f);
	static float constant_timestep=0.f;
	if(constant_timestep)
		dt=constant_timestep;
	if(simulWeatherRenderer)
		simulWeatherRenderer->Update(dt);
	
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
			if(simulWeatherRenderer->IsCloudLayer1Visible()&&simulWeatherRenderer->GetCloudRenderer())
			{
				simulTerrainRenderer->SetCloudTextures		(simulWeatherRenderer->GetCloudRenderer()->GetCloudTextures(),simulWeatherRenderer->GetCloudRenderer()->GetCloudInterface()->GetWrap());
				simulTerrainRenderer->SetCloudScales		(simulWeatherRenderer->GetCloudRenderer()->GetCloudScales());
				simulTerrainRenderer->SetCloudOffset		(simulWeatherRenderer->GetCloudRenderer()->GetCloudOffset());
				simulTerrainRenderer->setCloudInterpolation	(simulWeatherRenderer->GetCloudRenderer()->GetInterpolation());
			}
			else
			{
				simulTerrainRenderer->SetCloudTextures		(NULL,false);
			}
			if(simulWeatherRenderer->GetSkyRenderer())
				simulTerrainRenderer->setFadeInterpolation	(simulWeatherRenderer->GetSkyRenderer()->GetFadeInterp());
		}
	}
	timer.FinishTime();
	simul::math::FirstOrderDecay(update_timing,timer.TimeSum,1.f,fTimeStep);
}
	
void Direct3D9Renderer::SetShowCloudCrossSections(bool val)
{
	show_cloud_sections=val;
}

void Direct3D9Renderer::SetShowLightVolume(bool val)
{
	render_light_volume=val;
}
	
	
void Direct3D9Renderer::SetShowFades(bool val)
{
	show_fades=val;
}
	
void Direct3D9Renderer::SetCelestialDisplay(bool val)
{
	celestial_display=val;
}

void Direct3D9Renderer::SetShowTerrain(bool val)
{
	show_terrain=val;
}

void Direct3D9Renderer::SetShowMap(bool val)
{
	show_map=val;
}

void Direct3D9Renderer::OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep)
{
	static simul::base::Timer timer;
	timer.TimeSum=0.f;
	timer.StartTime();
	fTime;fTimeStep;
    if(!SUCCEEDED(pd3dDevice->BeginScene()))
		return;
	//pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF00FF00,1.0f,0L);

//since our skybox will blend based on alpha we have to clear the backbuffer to this alpha value
	D3DXCOLOR fogColor( 0.0f , 1.f , 1.f , 0.0f ); 
	// Don't need to clear D3DCLEAR_TARGET as we'll be filling every pixel:
	pd3dDevice->Clear(0L,NULL,D3DCLEAR_ZBUFFER|D3DCLEAR_TARGET,0xFFFF7000,1.0f,0L);

    pd3dDevice->SetRenderState( D3DRS_ZENABLE,FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,FALSE);

	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	pd3dDevice->SetTransform(D3DTS_WORLD,&ident);
	D3DXMATRIX view(camera->MakeViewMatrix(false));
	pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	// Make left-handed matrix if y is vertical
	D3DXMATRIX proj(camera->MakeProjectionMatrix(1.f,250000.f,aspect,y_vertical));
	pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
	if(simulHDRRenderer)
	{
		simulHDRRenderer->SetGamma(Gamma);
		simulHDRRenderer->SetExposure(Exposure);
		simulHDRRenderer->StartRender();
	}
	if(simulWeatherRenderer)
	{
#ifdef XBOX
		simulWeatherRenderer->SetMatrices(view,proj);
#endif
		simulWeatherRenderer->RenderSky(true,false);
	}
	if(simulTerrainRenderer&&show_terrain)
	{
		simulTerrainRenderer->SetMatrices(view,proj);
		simulTerrainRenderer->Render();
	}
	if(simulHDRRenderer)
		simulHDRRenderer->CopyDepthAlpha();
	timer.UpdateTime();
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetAtmosphericsRenderer())
			simulWeatherRenderer->GetAtmosphericsRenderer()->Render();
		simulWeatherRenderer->RenderFlares();
		pd3dDevice->SetTransform(D3DTS_VIEW,&view);
		simulWeatherRenderer->RenderLateCloudLayer(true);
		simulWeatherRenderer->RenderLightning();
		if(render_light_volume)
			simulWeatherRenderer->GetCloudRenderer()->RenderLightVolume();
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&celestial_display)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(width,height);
		simulWeatherRenderer->RenderPrecipitation();
	}
	timer.UpdateTime();
	simul::math::FirstOrderDecay(weather_timing,timer.Time,1.f,fTimeStep);
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&show_fades)
		simulWeatherRenderer->GetSkyRenderer()->RenderFades(width);

	if(simulHDRRenderer)
		simulHDRRenderer->FinishRender();
	timer.UpdateTime();
	simul::math::FirstOrderDecay(hdr_timing,timer.Time,1.f,fTimeStep);

	if(simulWeatherRenderer&&show_cloud_sections)
	{
		if(simulWeatherRenderer->IsCloudLayer1Visible())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(width);
		//	simulWeatherRenderer->GetCloudRenderer()->RenderDistances(width,height);
		}
		if(simulWeatherRenderer->IsCloudLayer2Visible())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(width);
		}
	}
	
	if(simulTerrainRenderer&&show_map)
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
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
}

void Direct3D9Renderer::OnDestroyDevice()
{
	OnLostDevice();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
//	if(simulHDRRenderer)
//		simulHDRRenderer->Destroy();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	/*if(simulHDRRenderer)
	{
		simulHDRRenderer=NULL;
	}
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer=NULL;
	}
	if(simulTerrainRenderer)
	{
		simulTerrainRenderer=NULL;
	}*/
	SAFE_RELEASE(m_pHudVertexDecl);
}

const TCHAR *Direct3D9Renderer::GetDebugText() const
{
	static TCHAR debug_text[512];
	if(!show_osd)
		return (_T(""));
	tstring weather_text;
	if(!simulWeatherRenderer.get())
		return (_T(""));
#ifdef _UNICODE
	weather_text=simul::base::StringToWString(simulWeatherRenderer->GetDebugText());
#else
	weather_text=simulWeatherRenderer->GetDebugText();
#endif
	if(simulWeatherRenderer)
		stprintf_s(debug_text,256,_T("WEATHER: %s\nFramerate %3.3g Render time %3.3g weather %3.3g hdr %3.3g\nUpdate time %3.3g"),weather_text.c_str(),framerate,render_timing,weather_timing,hdr_timing,update_timing);
	return debug_text;
}