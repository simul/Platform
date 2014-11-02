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
	,HiResDownscale(2)
	,lossTexture(0)
	,Width(0)
	,Height(0)
	,Downscale(0)
	,lowResFarFramebufferDx11(NULL)
	,lowResNearFramebufferDx11(NULL)
	,hiResFarFramebufferDx11(NULL)
	,hiResNearFramebufferDx11(NULL)
{
}

void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform	=r;
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(lowResFarFramebufferDx11);
	SAFE_DELETE(lowResNearFramebufferDx11);
	SAFE_DELETE(hiResFarFramebufferDx11);
	SAFE_DELETE(hiResNearFramebufferDx11);
	if(!renderPlatform)
		return;
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	lossTexture		=renderPlatform->CreateTexture();
	lowResFarFramebufferDx11	=renderPlatform->CreateFramebuffer();
	lowResNearFramebufferDx11	=renderPlatform->CreateFramebuffer();
	hiResFarFramebufferDx11		=renderPlatform->CreateFramebuffer();
	hiResNearFramebufferDx11	=renderPlatform->CreateFramebuffer();

	lowResFarFramebufferDx11	->SetFormat(crossplatform::RGBA_16_FLOAT);
	lowResNearFramebufferDx11	->SetFormat(crossplatform::RGBA_16_FLOAT);
	hiResFarFramebufferDx11		->SetFormat(crossplatform::RGBA_16_FLOAT);
	hiResNearFramebufferDx11	->SetFormat(crossplatform::RGBA_16_FLOAT);

	lowResFarFramebufferDx11	->SetDepthFormat(crossplatform::D_16_UNORM);
	lowResNearFramebufferDx11	->SetDepthFormat(crossplatform::UNKNOWN);
	hiResFarFramebufferDx11		->SetDepthFormat(crossplatform::UNKNOWN);
	hiResNearFramebufferDx11	->SetDepthFormat(crossplatform::UNKNOWN);
	lowResFarFramebufferDx11	->SetUseFastRAM(true,true);
	lowResNearFramebufferDx11	->SetUseFastRAM(true,true);
	hiResFarFramebufferDx11		->SetUseFastRAM(true,true);
	hiResNearFramebufferDx11	->SetUseFastRAM(true,true);
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth				=(Width+Downscale-1)/Downscale+1;
	int BufferHeight			=(Height+Downscale-1)/Downscale+1;
	int W						=(Width+HiResDownscale-1)/HiResDownscale+1;
	int H						=(Height+HiResDownscale-1)/HiResDownscale+1;
	lowResFarFramebufferDx11	->SetWidthAndHeight(BufferWidth,BufferHeight);
	lowResNearFramebufferDx11	->SetWidthAndHeight(BufferWidth,BufferHeight);
	hiResFarFramebufferDx11		->SetWidthAndHeight(W,H);
	hiResNearFramebufferDx11	->SetWidthAndHeight(W,H);
	// We're going to TRY to encode near and far loss into two UINT's, for faster results
	lossTexture->ensureTexture2DSizeAndFormat(renderPlatform,W,H,crossplatform::RGBA_32_UINT,false,true);
	lowResFarFramebufferDx11	->RestoreDeviceObjects(r);
	lowResNearFramebufferDx11	->RestoreDeviceObjects(r);
	hiResFarFramebufferDx11		->RestoreDeviceObjects(r);
	hiResNearFramebufferDx11	->RestoreDeviceObjects(r);
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE(lowResFarFramebufferDx11);
	SAFE_DELETE(lowResNearFramebufferDx11);
	SAFE_DELETE(hiResFarFramebufferDx11);
	SAFE_DELETE(hiResNearFramebufferDx11);
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
void TwoResFramebuffer::ActivateHiRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PushRenderTargets(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	if(!GetHiResFarFramebuffer()->IsValid())
		GetHiResFarFramebuffer()->CreateBuffers();
	if(!GetHiResNearFramebuffer()->IsValid())
		GetHiResNearFramebuffer()->CreateBuffers();
	crossplatform::Texture * targs[]={GetHiResFarFramebuffer()->GetTexture(),GetHiResNearFramebuffer()->GetTexture()};
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,NULL);
	int w=GetHiResFarFramebuffer()->Width
		,h=GetHiResFarFramebuffer()->Height;
	crossplatform::Viewport v[]={{0,0,w,h,0,1.f},{0,0,w,h,0,1.f}};
	renderPlatform->SetViewports(deviceContext,2,v);
}

void TwoResFramebuffer::DeactivateHiRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PopRenderTargets(deviceContext);
}

void TwoResFramebuffer::ActivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PushRenderTargets(deviceContext);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	if(!pContext)
		return;
	
	if(!GetLowResFarFramebuffer()->IsValid())
		GetLowResFarFramebuffer()->CreateBuffers();
	if(!GetLowResNearFramebuffer()->IsValid())
		GetLowResNearFramebuffer()->CreateBuffers();
	crossplatform::Texture * targs[]={GetLowResFarFramebuffer()->GetTexture(),GetLowResNearFramebuffer()->GetTexture()};
	crossplatform::Texture * depth=GetLowResFarFramebuffer()->GetDepthTexture();
	renderPlatform->ActivateRenderTargets(deviceContext,2,targs,depth);
	int w=GetLowResFarFramebuffer()->Width
		,h=GetLowResFarFramebuffer()->Height;
	crossplatform::Viewport v[]={{0,0,w,h,0,1.f},{0,0,w,h,0,1.f}};
	renderPlatform->SetViewports(deviceContext,2,v);
}

void TwoResFramebuffer::DeactivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->PopRenderTargets(deviceContext);
}

void TwoResFramebuffer::SetDimensions(int w,int h,int downscale,int hiResDownscale)
{
	if(downscale<1)
		downscale=1;
	if(hiResDownscale<1)
		hiResDownscale=1;
	if(Width!=w||Height!=h||Downscale!=downscale||HiResDownscale!=hiResDownscale)
	{
		Width=w;
		Height=h;
		Downscale=downscale;
		HiResDownscale=hiResDownscale;
		RestoreDeviceObjects(renderPlatform);
	}
}

void TwoResFramebuffer::GetDimensions(int &w,int &h,int &downscale,int &hiResDownscale)
{
	w=Width;
	h=Height;
	downscale=Downscale;
	hiResDownscale=HiResDownscale;
}