#include "BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
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

void BaseFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
}

void BaseFramebuffer::InvalidateDeviceObjects()
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


void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform	=r;
	SAFE_DELETE(lossTexture);
	lossTexture		=renderPlatform->CreateTexture();
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE(lossTexture);
}

void TwoResFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Texture * targs[]={GetLowResFarFramebuffer()->GetTexture(),GetLowResNearFramebuffer()->GetTexture()};
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,NULL);
}

crossplatform::Texture *TwoResFramebuffer::GetLossTexture()
{
	return lossTexture;
}