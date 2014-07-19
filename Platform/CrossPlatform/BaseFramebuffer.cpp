#include "BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
using namespace simul;
using namespace crossplatform;

BaseFramebuffer::BaseFramebuffer(int w,int h)
	:Width(w)
	,Height(h)
	,numAntialiasingSamples(1)	// no AA by default
	,depth_active(false)
	,colour_active(false)
	,renderPlatform(0)
{
}
bool BaseFramebuffer::IsDepthActive() const
{
	return depth_active;
}

bool BaseFramebuffer::IsColourActive() const
{
	return colour_active;
}


void TwoResFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Texture * targs[]={GetLowResFarFramebuffer()->GetTexture(),GetLowResNearFramebuffer()->GetTexture()};
	deviceContext.renderPlatform->ActivateRenderTargets(deviceContext,2,targs,NULL);
}
