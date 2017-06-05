#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/Crossplatform/Material.h"
#include "Simul/Platform/Crossplatform/DemoOverlay.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Terrain/BaseSeaRenderer.h"
#include "Simul/Platform/CrossPlatform/HDRRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Math/pi.h"
using namespace simul;
using namespace dx11;

Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m)
	:trueSkyRenderer(env,s,m)
{
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	trueSkyRenderer.InvalidateDeviceObjects();
}

void Direct3D11Renderer::OnCreateDevice(void* pd3dDevice)
{
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

void Direct3D11Renderer::ResizeView(int view_id,int w,int h)
{
	return trueSkyRenderer.ResizeView(view_id,w,h);
}

void Direct3D11Renderer::Render(int view_id,void* pd3dDevice,void* context)
{
	crossplatform::DeviceContext deviceContext;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	deviceContext.platform_context	=pContext;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatformDx11.GetGpuProfiler());
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=view_id;
	unsigned num=0;
	D3D11_VIEWPORT d3d11viewports[8];
	ID3D11RenderTargetView *pOldRenderTarget[] = { NULL,NULL,NULL,NULL };
	ID3D11DepthStencilView *pOldDepthSurface=NULL;
	pContext->RSGetViewports(&num,NULL);
	pContext->RSGetViewports(&num,d3d11viewports);
	pContext->OMGetRenderTargets(	4,
					pOldRenderTarget,
					&pOldDepthSurface
					);
	pContext->OMSetRenderTargets(1,pOldRenderTarget,NULL);
	
	simul::crossplatform::BaseFramebuffer::setDefaultRenderTargets(pOldRenderTarget[0],
															pOldDepthSurface,
															d3d11viewports[0].TopLeftX,
															d3d11viewports[0].TopLeftY,
															d3d11viewports[0].TopLeftX+d3d11viewports[0].Width,
															d3d11viewports[0].TopLeftY+d3d11viewports[0].Height
															);
	trueSkyRenderer.Render(deviceContext);
	
	pContext->OMSetRenderTargets(4,pOldRenderTarget,pOldDepthSurface);
	if(pOldRenderTarget[0])
		pOldRenderTarget[0]->Release();
	if(pOldDepthSurface)
		pOldDepthSurface->Release();
	pContext->RSSetViewports(1,d3d11viewports);
	
}