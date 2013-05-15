#include "Direct3D1xRenderer.h"

// Simul Weather:
#include "Simul/Platform/DirectX11/SimulWeatherRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulTerrainRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulHDRRendererDX1x.h"
#include "Simul/Platform/DirectX11/Simul2DCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulOpticsRendererDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"

Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,int w,int h):
		camera(NULL)
		,y_vertical(true)
		,UseHdrPostprocessor(true)
		,UseSkyBuffer(true)
		,ShowLightVolume(false)
		,CelestialDisplay(false)
		,enabled(true)
		,ShowWater(true)
		,MakeCubemap(true)
		,ShowCloudCrossSections(false)
		,ReverseDepth(false)
		,ShowOSD(false)
{
	simulWeatherRenderer=new SimulWeatherRendererDX1x(env,true,false,w,h,true,true,true);
	AddChild(simulWeatherRenderer.get());
	simulHDRRenderer=new SimulHDRRendererDX1x(128,128);
	simulOpticsRenderer=new SimulOpticsRendererDX1x();
	simulTerrainRenderer=new SimulTerrainRendererDX1x();
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	Group::RemoveChild(simulWeatherRenderer.get());
	simulWeatherRenderer=NULL;
}

// D3D11CallbackInterface
bool Direct3D11Renderer::IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed)
{
	return true;
}

bool Direct3D11Renderer::ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings)
{
	pDeviceSettings->d3d11.CreateFlags|=D3D11_CREATE_DEVICE_DEBUG;
	enabled=true;
	if(pDeviceSettings->d3d11.DriverType!=D3D_DRIVER_TYPE_HARDWARE)
		enabled=false;
    return true;
}

HRESULT	Direct3D11Renderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	gpuCloudGenerator.RestoreDeviceObjects(pd3dDevice);
	gpuSkyGenerator.RestoreDeviceObjects(pd3dDevice);
	return S_OK;
}

HRESULT	Direct3D11Renderer::OnD3D11ResizedSwapChain(	ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	if(!enabled)
		return S_OK;
	try
	{
		ScreenWidth=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;
		simul::dx11::UnsetDevice();
		//Set a global device pointer for use by various classes.
		simul::dx11::SetDevice(pd3dDevice);
		ScreenWidth=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;
		if(simulWeatherRenderer)
			simulWeatherRenderer->SetScreenSize(ScreenWidth,ScreenHeight);
		if(simulHDRRenderer)
			simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
		return S_OK;
	}
	catch(...)
	{
		return S_FALSE;
	}
}

void Direct3D11Renderer::OnD3D11FrameRender(ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)
{
	if(!enabled)
		return;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	D3DXMATRIX world,view,proj;
	static float nearPlane=0.01f;
	static float farPlane=250000.f;
	if(camera)
	{
		proj=camera->MakeProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight,false);
		view=camera->MakeViewMatrix(!y_vertical);
		D3DXMatrixIdentity(&world);
	}
	simul::dx11::UtilityRenderer::SetMatrices(view,proj);
	D3DXMatrixIdentity(&world);
	if(simulHDRRenderer&&UseHdrPostprocessor)
		simulHDRRenderer->StartRender(pd3dImmediateContext);
	if(simulWeatherRenderer)
	{
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		else
			simulWeatherRenderer->SetExposureHint(1.0f);
		simulWeatherRenderer->SetMatrices(view,proj);
		simulWeatherRenderer->RenderSky(pd3dImmediateContext,UseSkyBuffer,false);
		if(MakeCubemap)
			simulWeatherRenderer->RenderCubemap(pd3dImmediateContext);
		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->StartRender(pd3dImmediateContext);
	}
	// Render solid things here.
	if(simulTerrainRenderer)
	{
		if(simulWeatherRenderer)
			simulTerrainRenderer->SetMaxFadeDistanceKm(simulWeatherRenderer->GetBaseSkyRenderer()->GetSkyKeyframer()->GetMaxDistanceKm());
		simulTerrainRenderer->SetMatrices(view,proj);
		simulTerrainRenderer->Render(pd3dImmediateContext);	
	}
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->FinishRender(pd3dImmediateContext);
			
		simulWeatherRenderer->RenderLateCloudLayer(pd3dImmediateContext,true);
		simulWeatherRenderer->RenderLightning(pd3dImmediateContext);
		if(simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->DrawCubemap(pd3dImmediateContext,(ID3D1xShaderResourceView*	)simulWeatherRenderer->GetCubemap());
		simulWeatherRenderer->DoOcclusionTests();
	}
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->RenderPrecipitation(pd3dImmediateContext);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			if(simulWeatherRenderer->GetSkyRenderer())
			{
				dir=simulWeatherRenderer->GetSkyRenderer()->GetDirectionToSun();
				light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
				simulOpticsRenderer->SetMatrices(view,proj);
				float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
				float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
				simulOpticsRenderer->RenderFlare(pd3dImmediateContext,exp,dir,light);
			}
		}
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
		simulHDRRenderer->FinishRender(pd3dImmediateContext);
	if(simulWeatherRenderer)
	{
		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(pd3dImmediateContext,ScreenWidth,ScreenHeight);
		if(ShowCloudCrossSections)
		{
			if(simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(pd3dImmediateContext,ScreenWidth,ScreenHeight);
			//	simulWeatherRenderer->GetCloudRenderer()->RenderDistances(width,height);
			}
			if(simulWeatherRenderer->Get2DCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(pd3dImmediateContext,ScreenWidth,ScreenHeight);
			}
		}
		if(ShowOSD)
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(pd3dImmediateContext,ScreenWidth,ScreenHeight);
		}
	}
	if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
		simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(pd3dImmediateContext,ScreenWidth,ScreenHeight);
	
	Profiler::GetGlobalProfiler().EndFrame(pd3dImmediateContext);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	Profiler::GetGlobalProfiler().Uninitialize();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
	simul::dx11::UnsetDevice();
}

void Direct3D11Renderer::OnD3D11DestroyDevice()
{
	OnD3D11LostDevice();
	// We don't clear the renderers because InvalidateDeviceObjects has already handled DX-specific destruction
	// And after OnD3D11DestroyDevice we might go back to startup without destroying the renderer.
	simulWeatherRenderer=NULL;
	simulHDRRenderer=NULL;
}

void Direct3D11Renderer::OnD3D11ReleasingSwapChain()
{
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


void Direct3D11Renderer::SetYVertical(bool y)
{
	y_vertical=y;
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->SetYVertical(y);
}

void Direct3D11Renderer::RecompileShaders()
{
	simul::dx11::UtilityRenderer::RecompileShaders();
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer.get())
		simulOpticsRenderer->RecompileShaders();
	if(simulTerrainRenderer.get())
		simulTerrainRenderer->RecompileShaders();
	if(simulHDRRenderer.get())
		simulHDRRenderer->RecompileShaders();
	gpuCloudGenerator.RecompileShaders();
	gpuSkyGenerator.RecompileShaders();
//	if(simulTerrainRenderer.get())
//		simulTerrainRenderer->RecompileShaders();
}

void    Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
{
	// The weather renderer works in days, not seconds
	float game_timestep=fTimeStep/(24.f*60.f*60.f);
	if(simulWeatherRenderer)
		simulWeatherRenderer->Update();
}

const char *Direct3D11Renderer::GetDebugText() const
{
	const char *s=NULL;
	if(simulWeatherRenderer)
		s=simulWeatherRenderer->GetDebugText();
	static char str[200];
	sprintf_s(str,200,"DirectX 11\n%s",s?s:"");
	return str;
}
