#include "BaseFramebuffer.h"
#include "Simul/Base/RuntimeError.h"
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
	,GenerateMips(false)
	,is_cubemap(false)
	,current_face(-1)
	,target_format(crossplatform::RGBA_32_FLOAT)
	,depth_format(crossplatform::UNKNOWN)
	,bands(4)
	,activate_count(0)
	,sphericalHarmonicsEffect(NULL)
	,external_texture(false)
	,external_depth_texture(false)
{
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
			buffer_texture=renderPlatform->CreateTexture();
		if(!external_depth_texture)
			buffer_depth_texture=renderPlatform->CreateTexture();
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
	sphericalHarmonicsConstants.InvalidateDeviceObjects();
	sphericalHarmonics.InvalidateDeviceObjects();
	SAFE_DELETE(sphericalHarmonicsEffect);
	sphericalSamples.InvalidateDeviceObjects();
}

void BaseFramebuffer::SetWidthAndHeight(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		if(renderPlatform)
			InvalidateDeviceObjects();
	}
	is_cubemap=false;
}

void BaseFramebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==target_format)
		return;
	target_format=f;
	InvalidateDeviceObjects();
}

void BaseFramebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
	InvalidateDeviceObjects();
}

void BaseFramebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void BaseFramebuffer::SetAsCubemap(int w)
{
	SetWidthAndHeight(w,w);
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
	if(buffer_texture==colour&&buffer_depth_texture==depth)
		return;
	SAFE_DELETE(buffer_texture);
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
		buffer_texture=renderPlatform->CreateTexture();
	if(!buffer_depth_texture)
		buffer_depth_texture=renderPlatform->CreateTexture();
	static int quality=0;
	if(!external_texture&&target_format!=crossplatform::UNKNOWN)
	{
		if(!is_cubemap)
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,target_format,false,true,false,numAntialiasingSamples,quality);
		else
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform,Width,Height,6,target_format,false,true,true);
	}
	if(!external_depth_texture&&depth_format!=crossplatform::UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,depth_format,false,false,true,numAntialiasingSamples,quality);
	}
	// The table of coefficients.
	int s=(bands+1);
	if(s<4)
		s=4;
	sphericalHarmonics.InvalidateDeviceObjects();
	sphericalSamples.InvalidateDeviceObjects();
	sphericalHarmonicsConstants.RestoreDeviceObjects(renderPlatform);
	return true;
}

void BaseFramebuffer::CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext)
{
	if(!sphericalHarmonicsEffect)
		RecompileShaders();
	int num_coefficients=bands*bands;
	static int BLOCK_SIZE=1;
	static int sqrt_jitter_samples					=4;
	if(!sphericalHarmonics.count)
	{
		sphericalHarmonics.RestoreDeviceObjects(renderPlatform,num_coefficients,true);
		sphericalSamples.RestoreDeviceObjects(renderPlatform,sqrt_jitter_samples*sqrt_jitter_samples,true);
	}
	sphericalHarmonicsConstants.num_bands			=bands;
	sphericalHarmonicsConstants.sqrtJitterSamples	=sqrt_jitter_samples;
	sphericalHarmonicsConstants.numJitterSamples	=sqrt_jitter_samples*sqrt_jitter_samples;
	sphericalHarmonicsConstants.invNumJitterSamples	=1.0f/(float)sphericalHarmonicsConstants.numJitterSamples;
	static int seed = 0;
	sphericalHarmonicsConstants.randomSeed			= seed++;
	seed = seed % 10000;
	sphericalHarmonicsConstants.Apply(deviceContext);
	sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");
	crossplatform::EffectTechnique *clear		=sphericalHarmonicsEffect->GetTechniqueByName("clear");
	sphericalHarmonicsEffect->Apply(deviceContext,clear,0);
	renderPlatform->DispatchCompute(deviceContext,(num_coefficients+BLOCK_SIZE-1)/BLOCK_SIZE,1,1);
	sphericalHarmonicsEffect->Unapply(deviceContext);
	{
		// The table of 3D directional sample positions. sqrt_jitter_samples x sqrt_jitter_samples
		// We just fill this buffer_texture with random 3d directions.
		crossplatform::EffectTechnique *jitter=sphericalHarmonicsEffect->GetTechniqueByName("jitter");
		sphericalSamples.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "samplesBufferRW");
		sphericalHarmonicsEffect->Apply(deviceContext,jitter,0);
		int u = (sqrt_jitter_samples + BLOCK_SIZE - 1) / BLOCK_SIZE;
		renderPlatform->DispatchCompute(deviceContext, u, u, 1);
		sphericalHarmonicsEffect->UnbindTextures(deviceContext);
		sphericalHarmonicsEffect->SetUnorderedAccessView(deviceContext,"samplesBufferRW",NULL);
		sphericalHarmonicsEffect->Unapply(deviceContext);
	}

	crossplatform::EffectTechnique *tech	=sphericalHarmonicsEffect->GetTechniqueByName("encode");
	sphericalHarmonicsEffect->SetTexture(deviceContext,"cubemapTexture"	,buffer_texture);
	sphericalSamples.Apply(deviceContext, sphericalHarmonicsEffect, "samplesBuffer");
	sphericalSamples.Apply(deviceContext, sphericalHarmonicsEffect, "samplesBuffer");
	sphericalHarmonics.ApplyAsUnorderedAccessView(deviceContext, sphericalHarmonicsEffect, "targetBuffer");
	
	static bool sh_by_samples=false;
	sphericalHarmonicsEffect->Apply(deviceContext,tech,0);
	int n = sh_by_samples ? sphericalHarmonicsConstants.numJitterSamples : num_coefficients;
	int U = ((n) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	renderPlatform->DispatchCompute(deviceContext, U, 1, 1);
	sphericalHarmonicsEffect->UnbindTextures(deviceContext);
	sphericalHarmonicsConstants.Unbind(deviceContext);
	sphericalHarmonicsEffect->Unapply(deviceContext);
}

void BaseFramebuffer::RecompileShaders()
{
	SAFE_DELETE(sphericalHarmonicsEffect);
	if(!renderPlatform)
		return;
	sphericalHarmonicsEffect=renderPlatform->CreateEffect("spherical_harmonics");
	sphericalHarmonicsConstants.LinkToEffect(sphericalHarmonicsEffect,"SphericalHarmonicsConstants");
}