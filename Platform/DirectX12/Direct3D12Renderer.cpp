#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX12/Direct3D12Renderer.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/pi.h"
using namespace simul;
using namespace dx12;

Direct3D11Renderer::Direct3D11Renderer(crossplatform::RenderPlatform *r,simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m)
	:renderPlatform(r)
	,trueSkyRenderer(env,s,m)
{
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	OnLostDevice();
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

int	Direct3D11Renderer::AddView()
{
	static int view_id=0;
	trueSkyRenderer.AddView(view_id++);
	return view_id;
}

void Direct3D11Renderer::RemoveView	(int view_id)
{
	return trueSkyRenderer.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,int w,int h)
{
	viewSize[view_id]=int2(w,h);
	return trueSkyRenderer.ResizeView(view_id,w,h);
}

void Direct3D11Renderer::SetRenderDelegate(int view_id,crossplatform::RenderDelegate d)
{
	renderDelegate[view_id]=d;
}

void Direct3D11Renderer::RegisterStartupDelegate(crossplatform::StartupDeviceDelegate d)
{
	startupDeviceDelegates.push_back(d);
}

void Direct3D11Renderer::RegisterShutdownDelegate(crossplatform::ShutdownDeviceDelegate d)
{
	shutdownDeviceDelegates.push_back(d);
}

void Direct3D11Renderer::Render(int view_id,void* context,void* rendertarget)
{
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=context;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatform->GetGpuProfiler());
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	
	int2 vs=viewSize[view_id];
	deviceContext.setDefaultRenderTargets(rendertarget,
															NULL,
															0,0,vs.x,vs.y
															);
	if(renderDelegate[view_id])
		renderDelegate[view_id](deviceContext);
}