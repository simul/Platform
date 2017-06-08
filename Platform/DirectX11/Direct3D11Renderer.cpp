#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/pi.h"
using namespace simul;
using namespace dx11;

Direct3D11Renderer::Direct3D11Renderer(crossplatform::RenderPlatform *r,simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m)
	:renderPlatform(r)
	,trueSkyRenderer(env,s,m)
	,renderDelegate(nullptr)
{
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	trueSkyRenderer.InvalidateDeviceObjects();
}

void Direct3D11Renderer::OnCreateDevice(void* dev)
{
	device=dev;
	for(auto d:startupDeviceDelegates)
		d(device);
	renderPlatform->RestoreDeviceObjects(device);
	trueSkyRenderer.RestoreDeviceObjects(renderPlatform);
}

void Direct3D11Renderer::OnLostDevice()
{
	for(auto d:shutdownDeviceDelegates)
		d();
	trueSkyRenderer.InvalidateDeviceObjects();
	renderPlatform->InvalidateDeviceObjects();
}

int	Direct3D11Renderer::AddView(bool external_fb)
{
	return trueSkyRenderer.AddView(external_fb);
}

void Direct3D11Renderer::RemoveView	(int view_id)
{
	return trueSkyRenderer.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,int w,int h)
{
	return trueSkyRenderer.ResizeView(view_id,w,h);
}

void Direct3D11Renderer::SetRenderDelegate(crossplatform::RenderDelegate d)
{
	renderDelegate=d;
}
void Direct3D11Renderer::RegisterStartupDelegate(crossplatform::StartupDeviceDelegate d)
{
	startupDeviceDelegates.push_back(d);
}
void Direct3D11Renderer::RegisterShutdownDelegate(crossplatform::ShutdownDeviceDelegate d)
{
	shutdownDeviceDelegates.push_back(d);
}

void Direct3D11Renderer::Render(int view_id,void* pd3dDevice,void* context)
{
	crossplatform::DeviceContext deviceContext;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)context;
	deviceContext.platform_context	=pContext;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatform->GetGpuProfiler());
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	unsigned num=0;
	D3D11_VIEWPORT d3d11viewports[8];
	ID3D11RenderTargetView *pOldRenderTarget[] = { NULL,NULL,NULL,NULL };
	ID3D11DepthStencilView *pOldDepthSurface=NULL;
	pContext->RSGetViewports(&num,NULL);
	pContext->RSGetViewports(&num,d3d11viewports);
	pContext->OMGetRenderTargets(4,pOldRenderTarget,&pOldDepthSurface);
	pContext->OMSetRenderTargets(1,pOldRenderTarget,NULL);
	
	simul::crossplatform::BaseFramebuffer::setDefaultRenderTargets(pOldRenderTarget[0],
															pOldDepthSurface,
															d3d11viewports[0].TopLeftX,
															d3d11viewports[0].TopLeftY,
															d3d11viewports[0].TopLeftX+d3d11viewports[0].Width,
															d3d11viewports[0].TopLeftY+d3d11viewports[0].Height
															);
	if(renderDelegate)
		renderDelegate(deviceContext);
	
	pContext->OMSetRenderTargets(4,pOldRenderTarget,pOldDepthSurface);
	if(pOldRenderTarget[0])
		pOldRenderTarget[0]->Release();
	if(pOldDepthSurface)
		pOldDepthSurface->Release();
	pContext->RSSetViewports(1,d3d11viewports);
	
}