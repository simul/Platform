#include "Direct3D1xRenderer.h"

// Simul Weather:
#include "Simul/Platform/DirectX 1x/SimulWeatherRendererDX1x.h"
//#include "Simul/Platform/Windows/DirectX 11/SimulTerrainRenderer.h"
#include "Simul/Platform/DirectX 1x/SimulCloudRendererDX1x.h"
#include "Simul/Platform/DirectX 1x/SimulHDRRendererDX1x.h"
//#include "Simul/Platform/Windows/DirectX 11/Simul2DCloudRendererDX1x.h"
#include "Simul/Platform/DirectX 1x/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX 1x/SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX 1x/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX 1x/MacrosDX1x.h"
#include "Simul/Graph/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"

Direct3D11Renderer::Direct3D11Renderer():
	camera(NULL)
{
	simulWeatherRenderer=new SimulWeatherRendererDX1x(true,640,360,true,true,false,true,false);
	AddChild(simulWeatherRenderer.get());
	simulHDRRenderer=new SimulHDRRendererDX1x(128,128);
	if(simulHDRRenderer)
		simulHDRRenderer->SetAtmospherics(simulWeatherRenderer->GetAtmosphericsRenderer());
}

Direct3D11Renderer::~Direct3D11Renderer()
{
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
	simul::dx11::SetShaderPath("MEDIA/HLSL/DX11");
	simul::dx11::SetTexturePath("MEDIA/Textures");

	unsigned ScreenWidth=pBackBufferSurfaceDesc->Width;
	unsigned ScreenHeight=pBackBufferSurfaceDesc->Height;
	// Create the HDR renderer to perform brightness and gamma-correction (optional component)
	simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
	// Set Always Render Clouds Late to true - clouds thru mountains.
	simulWeatherRenderer=new SimulWeatherRendererDX1x(false,false,ScreenWidth/2,ScreenHeight/2,true,true,false,true);
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
	if(simulHDRRenderer&&!simulHDRRenderer->RestoreDeviceObjects(pd3dDevice,pSwapChain))
		return S_FALSE;
	if(simulWeatherRenderer&&!simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice,pSwapChain))
		return S_FALSE;
}

void	Direct3D11Renderer::OnD3D11FrameRender(			ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)
{
	D3DXMATRIX world,view,proj;
	if(camera)
	{
		proj=camera->MakeProjectionMatrix(.1,250000.f,1.f,true);
		view=camera->MakeViewMatrix(false);
		D3DXMatrixIdentity(&world);
	}
	if(simulHDRRenderer)
		simulHDRRenderer->StartRender();
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices(view,proj);
		simulWeatherRenderer->RenderSky(false,false);
	}
	if(simulHDRRenderer)
		simulHDRRenderer->FinishRender();
}

void	Direct3D11Renderer::OnD3D11LostDevice()
{
}

void	Direct3D11Renderer::OnD3D11DestroyDevice()
{
    simulWeatherRenderer=NULL;
    simulHDRRenderer=NULL;
	simul::dx11::UnsetDevice();
}

void	Direct3D11Renderer::OnD3D11ReleasingSwapChain()
{
}

bool	Direct3D11Renderer::OnDeviceRemoved()
{
	return true;
}

void    Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
{
	static float timeMult=100.f;
	// The weather renderer works in days, not seconds
	float game_timestep=timeMult*fTimeStep/(24.f*60.f*60.f);
	if(simulWeatherRenderer)
		simulWeatherRenderer->Update(game_timestep);
}

const char *	Direct3D11Renderer::GetDebugText() const
{
	return "";
}
