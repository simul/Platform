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
	,frame(0)
{
}

RenderDelegater::~RenderDelegater()
{
	OnLostDevice();
}

void RenderDelegater::SetRenderPlatform(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
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

void RenderDelegater::RegisterShutdownDelegate(crossplatform::ShutdownDeviceDelegate d)
{
	shutdownDeviceDelegates.push_back(d);
}

void RenderDelegater::Render(int view_id,void* context,void* rendertarget,int w,int h)
{
	if(!rendertarget)
		return;
	crossplatform::DeviceContext deviceContext;
	viewSize[view_id]					= int2(w,h);
	deviceContext.platform_context		= context;
	deviceContext.renderPlatform		= renderPlatform;
	deviceContext.viewStruct.view_id	= view_id;
	deviceContext.frame_number			= frame++;
	int2 vs								= viewSize[view_id];

	std::string pn(renderPlatform->GetName());
	deviceContext.setDefaultRenderTargets
		(
			rendertarget, NULL,
			0, 0, vs.x, vs.y
		);

	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatform->GetGpuProfiler());
	if (renderDelegate[view_id])
	{
		renderDelegate[view_id](deviceContext);
	}
}