#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/RenderDelegater.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/renderPlatform.h"
#include "Simul/Math/pi.h"
using namespace simul;
using namespace crossplatform;

RenderDelegater::RenderDelegater(crossplatform::RenderPlatform *r,simul::base::MemoryInterface *m)
	:device(nullptr)
	,last_view_id(0)
	,renderPlatform(r)
{
}

RenderDelegater::~RenderDelegater()
{
	OnLostDevice();
}

void RenderDelegater::OnCreateDevice(void* dev)
{
	device=dev;
	for(auto d:startupDeviceDelegates)
		d(device);
	renderPlatform->RestoreDeviceObjects(device);
}

void RenderDelegater::OnLostDevice()
{
	for(auto d:shutdownDeviceDelegates)
		d();
	renderPlatform->InvalidateDeviceObjects();
}

int	RenderDelegater::AddView()
{
	return last_view_id++;
}

void RenderDelegater::RemoveView	(int view_id)
{
}

void RenderDelegater::ResizeView(int view_id,int w,int h)
{
}

void RenderDelegater::SetRenderDelegate(int view_id,crossplatform::RenderDelegate d)
{
	renderDelegate[view_id]=d;
}

void RenderDelegater::RegisterStartupDelegate(crossplatform::StartupDeviceDelegate d)
{
	startupDeviceDelegates.push_back(d);
}

void RenderDelegater::RegisterShutdownDelegate(crossplatform::ShutdownDeviceDelegate d)
{
	shutdownDeviceDelegates.push_back(d);
}

void RenderDelegater::Render(int view_id,void* context,void* rendertarget,int w,int h)
{
	viewSize[view_id]=int2(w,h);
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
	if (renderDelegate[view_id])
	{
		renderDelegate[view_id](deviceContext);
	}
}