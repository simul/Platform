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
	,buffer_texture(NULL)
	,buffer_depth_texture(NULL)
	,is_cubemap(false)
	,current_face(-1)
	,GenerateMips(false)
	,target_format(crossplatform::RGBA_32_FLOAT)
	,depth_format(crossplatform::UNKNOWN)
	,bands(4)
	,activate_count(0)
	,sphericalHarmonicsEffect(NULL)
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
