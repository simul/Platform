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

TwoResFramebuffer::TwoResFramebuffer()
	:renderPlatform(0)
	,lossTexture(NULL)
	,Width(0)
	,Height(0)
	,Downscale(0)
{
	volumeTextures[0] = volumeTextures[1] = NULL;
	for (int i = 0; i < 3; i++)
		lowResFramebuffers[i] = NULL;
}

void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform	=r;
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(volumeTextures[0]);
	SAFE_DELETE(volumeTextures[1]);
	for (int i = 0; i < 3; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	if(!renderPlatform)
		return;
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	lossTexture		=renderPlatform->CreateTexture();
	volumeTextures[0]		=renderPlatform->CreateTexture();
	volumeTextures[1] = renderPlatform->CreateTexture();
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth = (Width + Downscale - 1) / Downscale + 1;
	int BufferHeight = (Height + Downscale - 1) / Downscale + 1;
	for (int i = 0; i < 3; i++)
	{
		lowResFramebuffers[i] = renderPlatform->CreateFramebuffer();
		lowResFramebuffers[i]->SetFormat(crossplatform::RGBA_16_FLOAT);
		lowResFramebuffers[i]->SetDepthFormat(crossplatform::UNKNOWN);
		lowResFramebuffers[i]->SetUseFastRAM(true, true);
		lowResFramebuffers[i]->SetWidthAndHeight(BufferWidth, BufferHeight);
		lowResFramebuffers[i]->RestoreDeviceObjects(r);
	}

	lowResFramebuffers[0]	->SetDepthFormat(crossplatform::D_16_UNORM);
	// We're going to TRY to encode near and far loss into two UINT's, for faster results
	lossTexture->ensureTexture2DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,crossplatform::RGBA_16_FLOAT,false,true);
	volumeTextures[0]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,false,1,true);
	volumeTextures[1]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,false,1,true);
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	for (int i = 0; i < 3; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(volumeTextures[0]);
	SAFE_DELETE(volumeTextures[1]);
}

void TwoResFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Texture * targs[] = { GetLowResFramebuffer(0)->GetTexture(), GetLowResFramebuffer(1)->GetTexture(), GetLowResFramebuffer(2)->GetTexture() };
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,NULL);
}

crossplatform::Texture *TwoResFramebuffer::GetLossTexture()
{
	return lossTexture;
}

crossplatform::Texture *TwoResFramebuffer::GetVolumeTexture(int num)
{
	return volumeTextures[num];
}

void TwoResFramebuffer::ActivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PushRenderTargets(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	for (int i = 0; i < 3; i++)
		if(!GetLowResFramebuffer(i)->IsValid())
			GetLowResFramebuffer(i)->CreateBuffers();
	crossplatform::Texture * targs[] = { GetLowResFramebuffer(0)->GetTexture(), GetLowResFramebuffer(1)->GetTexture(), GetLowResFramebuffer(2)->GetTexture() };
	crossplatform::Texture * depth = GetLowResFramebuffer(0)->GetDepthTexture();
	renderPlatform->ActivateRenderTargets(deviceContext,3,targs,depth);
	int w=GetLowResFramebuffer(0)->Width
		, h = GetLowResFramebuffer(0)->Height;
	crossplatform::Viewport v[]={{0,0,w,h,0,1.f},{0,0,w,h,0,1.f}};
	renderPlatform->SetViewports(deviceContext,2,v);
}

void TwoResFramebuffer::DeactivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PopRenderTargets(deviceContext);
}

void TwoResFramebuffer::ActivateVolume(crossplatform::DeviceContext &deviceContext,int num)
{
	renderPlatform->PushRenderTargets(deviceContext);
	// activate all of the rt's of this texture at once.
	volumeTextures[num]->activateRenderTarget(deviceContext);
	int w = GetLowResFramebuffer(0)->Width, h = GetLowResFramebuffer(0)->Height;
	crossplatform::Viewport v[]={{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f}};
	renderPlatform->SetViewports(deviceContext,volumeTextures[num]->depth,v);
}

void TwoResFramebuffer::DeactivateVolume(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PopRenderTargets(deviceContext);
}

void TwoResFramebuffer::SetDimensions(int w,int h,int downscale)
{
	if(downscale<1)
		downscale=1;
	if(Width!=w||Height!=h||Downscale!=downscale)
	{
		Width=w;
		Height=h;
		Downscale=downscale;
		RestoreDeviceObjects(renderPlatform);
	}
}

void TwoResFramebuffer::GetDimensions(int &w,int &h,int &downscale)
{
	w=Width;
	h=Height;
	downscale=Downscale;
}