#define NOMINMAX
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Math/Pi.h"
using namespace simul;
using namespace crossplatform;

RenderDelegater::RenderDelegater(crossplatform::RenderPlatform *r)
	:last_view_id(0)
	,renderPlatform(r)
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
	renderPlatform = nullptr;
	shutdownDeviceDelegates.clear();
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

void RenderDelegater::Render(int view_id,void* context,void* rendertarget,int w,int h, long long f)
{
	//if(!rendertarget)
	//	return;
	ERRNO_BREAK
	crossplatform::DeviceContext deviceContext;
	viewSize[view_id]					= int2(w,h);
	deviceContext.platform_context		= context;
	deviceContext.renderPlatform		= renderPlatform;
	deviceContext.viewStruct.view_id	= view_id;
	deviceContext.frame_number			= f;
	crossplatform::Viewport vps[1];
	vps[0].x = vps[0].y = 0;
	vps[0].w = w;
	vps[0].h = h;
	renderPlatform->SetViewports(deviceContext, 1, vps);
	int2 vs	= viewSize[view_id];

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
