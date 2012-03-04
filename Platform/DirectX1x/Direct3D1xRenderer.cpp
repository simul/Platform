#include "Direct3D1xRenderer.h"

// Simul Weather:
#include "Simul/Platform/DirectX1x/SimulWeatherRendererDX1x.h"
//#include "Simul/Platform/Windows/DirectX 11/SimulTerrainRenderer.h"
#include "Simul/Platform/DirectX1x/SimulCloudRendererDX1x.h"
#include "Simul/Platform/DirectX1x/SimulHDRRendererDX1x.h"
//#include "Simul/Platform/Windows/DirectX 11/Simul2DCloudRendererDX1x.h"
#include "Simul/Platform/DirectX1x/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX1x/SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX1x/SimulOpticsRendererDX1x.h"
#include "Simul/Platform/DirectX1x/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX1x/MacrosDX1x.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"

Direct3D11Renderer::Direct3D11Renderer(const char *license_key,simul::clouds::CloudKeyframer *cloudKeyframer):
		camera(NULL)
		,timeMult(0.f)
		,y_vertical(true)
{
	simulWeatherRenderer=new SimulWeatherRendererDX1x(license_key,cloudKeyframer,true,false,640,360,true,true,true);
	AddChild(simulWeatherRenderer.get());
	//simulHDRRenderer=new SimulHDRRendererDX1x(128,128);
	simulOpticsRenderer=new SimulOpticsRendererDX1x();
	if(simulOpticsRenderer)
		simulOpticsRenderer->SetYVertical(y_vertical);
	SetYVertical(y_vertical);
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	Group::RemoveChild(simulWeatherRenderer.get());
	simulWeatherRenderer=NULL;
}

// D3D11CallbackInterface
bool	Direct3D11Renderer::IsD3D11DeviceAcceptable(	const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed)
{
	return true;
}

bool	Direct3D11Renderer::ModifyDeviceSettings(		DXUTDeviceSettings* pDeviceSettings)
{
	pDeviceSettings->d3d11.CreateFlags|=D3D11_CREATE_DEVICE_DEBUG;
    return true;
}

HRESULT	Direct3D11Renderer::OnD3D11CreateDevice(		ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	unsigned ScreenWidth=pBackBufferSurfaceDesc->Width;
	unsigned ScreenHeight=pBackBufferSurfaceDesc->Height;
	aspect=(float)ScreenWidth/(float)ScreenHeight;
	// Create the HDR renderer to perform brightness and gamma-correction (optional component)
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
	// Set Always Render Clouds Late to true - clouds thru mountains.
	// Callback to fill lo-res depth buffer for clouds
	//if(simulWeatherRenderer)
//		simulWeatherRenderer->SetRenderDepthBufferCallback(&cb);
	return S_OK;
}

HRESULT	Direct3D11Renderer::OnD3D11ResizedSwapChain(	ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	simul::dx11::UnsetDevice();
	//Set a global device pointer for use by various classes.
	simul::dx11::SetDevice(pd3dDevice);
	unsigned ScreenWidth=pBackBufferSurfaceDesc->Width;
	unsigned ScreenHeight=pBackBufferSurfaceDesc->Height;
	aspect=(float)ScreenWidth/(float)ScreenHeight;
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer&&!simulHDRRenderer->RestoreDeviceObjects(pd3dDevice,pSwapChain))
		return (HRESULT)(-1);
	if(simulWeatherRenderer&&!simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice,pSwapChain))
		return (HRESULT)(-1);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	return S_OK;
}

void	Direct3D11Renderer::OnD3D11FrameRender(			ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)
{
	D3DXMATRIX world,view,proj;
	static float nr=0.01f;
	static float fr=250000.f;
	if(camera)
	{
		proj=camera->MakeProjectionMatrix(nr,fr,aspect,y_vertical);
		//D3DXMatrixPerspectiveFovRH(&proj,camera->GetVerticalFieldOfViewDegrees()*3.141f/180.f,aspect,.1f,250000.f);
		view=camera->MakeViewMatrix(!y_vertical);
		D3DXMatrixIdentity(&world);
	}
	if(simulHDRRenderer)
		simulHDRRenderer->StartRender();
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices(view,proj);
		simulWeatherRenderer->RenderSky(true,false);
//		if(ShowFades&&simulWeatherRenderer->GetSkyRenderer())
//			simulWeatherRenderer->GetSkyRenderer()->RenderFades();
		simulWeatherRenderer->DoOcclusionTests();
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			dir=simulWeatherRenderer->GetSkyRenderer()->GetDirectionToLight();
			light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
		simulOpticsRenderer->SetMatrices(view,proj);
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-(simulWeatherRenderer?simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion():0.f));
			simulOpticsRenderer->RenderFlare(exp,dir,light);
		}
	}
	if(simulHDRRenderer)
		simulHDRRenderer->FinishRender();
}

void	Direct3D11Renderer::OnD3D11LostDevice()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
}

void	Direct3D11Renderer::OnD3D11DestroyDevice()
{
	OnD3D11LostDevice();
	// We don't clear the renderers because InvalidateDeviceObjects has already handled DX-specific destruction
	// And after OnD3D11DestroyDevice we might go back to startup without destroying the renderer.
	simulWeatherRenderer=NULL;
	simulHDRRenderer=NULL;
	simul::dx11::UnsetDevice();
}

void	Direct3D11Renderer::OnD3D11ReleasingSwapChain()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
}

bool	Direct3D11Renderer::OnDeviceRemoved()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	return true;
}


void Direct3D11Renderer::SetYVertical(bool y)
{
	y_vertical=y;
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->SetYVertical(y);
	//if(simulTerrainRenderer.get())
	//	simulTerrainRenderer->SetYVertical(y_vertical);
	if(simulOpticsRenderer)
		simulOpticsRenderer->SetYVertical(y_vertical);
	if(simulOpticsRenderer)
		simulOpticsRenderer->SetYVertical(y_vertical);
}

void Direct3D11Renderer::RecompileShaders()
{
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer.get())
		simulOpticsRenderer->RecompileShaders();
	if(simulHDRRenderer.get())
		simulHDRRenderer->RecompileShaders();
//	if(simulTerrainRenderer.get())
//		simulTerrainRenderer->RecompileShaders();
}

void    Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
{
	// The weather renderer works in days, not seconds
	float game_timestep=timeMult*fTimeStep/(24.f*60.f*60.f);
	if(simulWeatherRenderer)
		simulWeatherRenderer->Update(game_timestep);
}

const char *	Direct3D11Renderer::GetDebugText() const
{
	return "";
}
