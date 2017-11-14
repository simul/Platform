#include "BaseFramebuffer.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Math/RandomNumberGenerator.h"
using namespace simul;
using namespace crossplatform;


BaseFramebuffer::BaseFramebuffer(const char *n)
	:Width(0)
	,Height(0)
	,mips(0)
	,numAntialiasingSamples(1)	// no AA by default
	,depth_active(false)
	,colour_active(false)
	,renderPlatform(0)
	,buffer_texture(NULL)
	,buffer_depth_texture(NULL)
	,GenerateMips(false)
	,DefaultClearColour(1.0f, 1.0f, 1.0f, 1.0f)
	,DefaultClearDepth(1.0f)
	,DefaultClearStencil(1)
	,is_cubemap(false)
	,current_face(-1)
	,target_format(crossplatform::RGBA_32_FLOAT)
	,depth_format(crossplatform::UNKNOWN)
	,activate_count(0)
	,external_texture(false)
	,external_depth_texture(false)
{
	if(n)
		name=n;
}

BaseFramebuffer::~BaseFramebuffer()
{
	InvalidateDeviceObjects();
}

void BaseFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	if(renderPlatform)
	{
		if(!external_texture)
			buffer_texture=renderPlatform->CreateTexture("BaseFramebufferColour");
		if(!external_depth_texture)
			buffer_depth_texture=renderPlatform->CreateTexture("BaseFramebufferDepth");
	}
	CreateBuffers();
}

void BaseFramebuffer::InvalidateDeviceObjects()
{
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=NULL;
	buffer_depth_texture=NULL;
}

void BaseFramebuffer::SetWidthAndHeight(int w,int h,int m)
{
	if(Width!=w||Height!=h||mips!=m)
	{
		Width=w;
		Height=h;
		mips=m;
		if(buffer_texture)
			buffer_texture->InvalidateDeviceObjects();
		if(buffer_depth_texture)
			buffer_depth_texture->InvalidateDeviceObjects();
	}
}

void BaseFramebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==target_format)
		return;
	target_format=f;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
}

void BaseFramebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
}

void BaseFramebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void BaseFramebuffer::SetAsCubemap(int w,int num_mips,crossplatform::PixelFormat f)
{
	SetWidthAndHeight(w,w,num_mips);
	SetFormat(f);
	is_cubemap=true;
}

void BaseFramebuffer::SetCubeFace(int f)
{
	if(!is_cubemap)
	{
		SIMUL_BREAK_ONCE("Setting cube face on non-cubemap framebuffer");
	}
	current_face=f;
}

bool BaseFramebuffer::IsDepthActive() const
{
	return depth_active;
}

bool BaseFramebuffer::IsColourActive() const
{
	return colour_active;
}

bool BaseFramebuffer::IsValid() const
{
	bool ok=(buffer_texture!=NULL)||(buffer_depth_texture!=NULL);
	return ok;
}

void BaseFramebuffer::SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth)
{
	if(buffer_texture==colour&&buffer_depth_texture==depth&&(!colour||(colour->width==Width&&colour->length==Height)))
		return;
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=colour;
	buffer_depth_texture=depth;

	external_texture=true;
	external_depth_texture=true;
	if(colour)
	{
		Width=colour->width;
		Height=colour->length;
		is_cubemap=(colour->depth==6);
	}
	else if(depth)
	{
		Width=depth->width;
		Height=depth->length;
	}
}

bool BaseFramebuffer::CreateBuffers()
{
	if(!Width||!Height)
		return false;
	if(!renderPlatform)
	{
		SIMUL_BREAK("renderPlatform should not be NULL here");
	}
	if(!renderPlatform)
		return false;
	if((buffer_texture&&buffer_texture->IsValid()))
		return true;
	if(buffer_depth_texture&&buffer_depth_texture->IsValid())
		return true;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(!buffer_texture)
	{
		std::string cName = "BaseFramebufferColour" + name;
		buffer_texture=renderPlatform->CreateTexture(cName.c_str());
	}
	if(!buffer_depth_texture)
	{
		std::string dName = "BaseFramebufferDepth" + name;
		buffer_depth_texture=renderPlatform->CreateTexture(dName.c_str());
	}
	static int quality=0;
	if(!external_texture&&target_format!=crossplatform::UNKNOWN)
	{
		if(!is_cubemap)
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,target_format,false,true,false,numAntialiasingSamples,quality,false,DefaultClearColour,DefaultClearDepth,DefaultClearStencil);
		else
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform,Width,Height,1,mips,target_format,false,true,true);
	}
	if(!external_depth_texture&&depth_format!=crossplatform::UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, depth_format, false, false, true, numAntialiasingSamples, quality, false, vec4(0.0f), DefaultClearDepth,DefaultClearStencil);
	}
	return true;
}
