#define NOMINMAX
#include "Simul/Platform/CrossPlatform/TwoResFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Math/Vector3.h"

using namespace simul;
using namespace crossplatform;

TwoResFramebuffer::TwoResFramebuffer()
	:renderPlatform(0)
	,lossTexture(NULL)
	,Width(0)
	,Height(0)
	,Downscale(0)
	,pixelOffset(0.f,0.f)
	,depthFormat(RGBA_16_FLOAT)
	,final_octave(0)
{
	volumeTextures[0] = volumeTextures[1] = NULL;
	for (int i = 0; i < 3; i++)
		lowResFramebuffers[i] = NULL;
	for(int i=0;i<4;i++)
		nearFarTextures[i]=NULL;
}

 
crossplatform::Texture *TwoResFramebuffer::GetLowResDepthTexture(int idx)
{
	return nearFarTextures[idx>=0?idx:(std::max(0,final_octave-1))];
}

crossplatform::PixelFormat TwoResFramebuffer::GetDepthFormat() const
{
	return depthFormat;
}

void TwoResFramebuffer::SetDepthFormat(crossplatform::PixelFormat p)
{
	depthFormat=p;
}

void TwoResFramebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	ERRNO_CHECK
	renderPlatform	=r;
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(volumeTextures[0]);
	SAFE_DELETE(volumeTextures[1]);
	for (int i = 0; i < 3; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	if(!renderPlatform)
		return;
	for (int i = 0; i < 3; i++)
	{
		lowResFramebuffers[i] = renderPlatform->CreateFramebuffer();
		lowResFramebuffers[i]->SetFormat(crossplatform::RGBA_16_FLOAT);
		lowResFramebuffers[i]->SetDepthFormat(crossplatform::UNKNOWN);
		lowResFramebuffers[i]->SetUseFastRAM(true, true);
	}
	lowResFramebuffers[2]->SetFormat(crossplatform::RGBA_32_FLOAT);
	lowResFramebuffers[0]	->SetDepthFormat(crossplatform::D_16_UNORM);
	ERRNO_CHECK
	for(int i=0;i<4;i++)
	{
		SAFE_DELETE(nearFarTextures[i]);
		nearFarTextures[i]=renderPlatform->CreateTexture("ESRAM");
		nearFarTextures[i]->MoveToFastRAM();
	}
	lossTexture			=renderPlatform->CreateTexture();
	volumeTextures[0]	=renderPlatform->CreateTexture();
	volumeTextures[1]	= renderPlatform->CreateTexture();
	ERRNO_CHECK
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth = (Width + Downscale - 1) / Downscale + 1;
	int BufferHeight = (Height + Downscale - 1) / Downscale + 1;
	ERRNO_CHECK
	for (int i = 0; i < 3; i++)
	{
		lowResFramebuffers[i]->SetWidthAndHeight(BufferWidth, BufferHeight);
		lowResFramebuffers[i]->RestoreDeviceObjects(r);
	}
	ERRNO_CHECK
	// We're going to TRY to encode near and far loss into two UINT's, for faster results
	lossTexture->ensureTexture2DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,crossplatform::RGBA_16_FLOAT,false,true);
	volumeTextures[0]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,false,1,true);
	volumeTextures[1]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,false,1,true);
	ERRNO_CHECK
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	for (int i = 0; i < 3; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	for(int i=0;i<4;i++)
		SAFE_DELETE(nearFarTextures[i]);
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


vec2 WrapOffset(vec2 pixelOffset,int scale)
{
	if(scale<1)
		scale=1;
	pixelOffset.x/=(float)scale;
	pixelOffset.y/=(float)scale;
	vec2 intOffset;
	pixelOffset.x = modf (pixelOffset.x , &intOffset.x);
	if(pixelOffset.x<0.0f)
		pixelOffset.x +=1.0f;
	pixelOffset.y = modf (pixelOffset.y , &intOffset.y);
	if(pixelOffset.y<0.0f)
		pixelOffset.y +=1.0f;
	
	pixelOffset.x*=(float)scale;
	pixelOffset.y*=(float)scale;
	return pixelOffset;
}

void TwoResFramebuffer::UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct,int scale)
{
	using namespace math;
	// Update the orientation due to changing view_dir:
	Vector3 cam_pos,new_view_dir,new_view_dir_local,new_up_dir;
	simul::crossplatform::GetCameraPosVector(viewStruct.view,cam_pos,new_view_dir,new_up_dir);
	new_view_dir.Normalize();
	view_o.GlobalToLocalDirection(new_view_dir_local,new_view_dir);
	float dx			= new_view_dir*view_o.Tx();
	float dy			= new_view_dir*view_o.Ty();
	dx*=Width*viewStruct.proj._11;
	dy*=Height*viewStruct.proj._22;
	view_o.DefineFromYZ(new_up_dir,new_view_dir);
	static float cc=0.5f;
	pixelOffset.x-=cc*dx;
	pixelOffset.y-=cc*dy;

	pixelOffset=WrapOffset(pixelOffset,scale);
}
void TwoResFramebuffer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const crossplatform::Viewport *viewport,int x0,int y0,int dx,int dy)
{
	vec4 white(1.0f,1.0f,1.0f,1.0f);
	vec4 black_transparent(0.0f,0.0f,0.0f,0.5f);
	int w		=dx/2;
	int l		=0;
	if(GetLowResDepthTexture()->width>0)
		l		=(GetLowResDepthTexture()->length*w)/GetLowResDepthTexture()->width;
	if(l==0&&depthTexture&&depthTexture->width)
	{
		l		=(depthTexture->length*w)/depthTexture->width;
	}
	if(l>dy/20)
	{
		l			=dy/2;
		if(GetLowResDepthTexture()->length>0)
			w		=(GetLowResDepthTexture()->width*l)/GetLowResDepthTexture()->length;
		else if(depthTexture&&depthTexture->length)
			w		=(depthTexture->width*l)/depthTexture->length;
	}
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0		,y0		,w,l,depthTexture,viewport);
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0		,"Main Depth",white,black_transparent);
	int x=x0;
	int y=y0+l;
	for(int i=0;i<4;i++)
	{
		crossplatform::Texture *t=GetLowResDepthTexture(i);
		if(!t)
			continue;
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x	,y	,w,l,	t);
		deviceContext.renderPlatform->Print(deviceContext		,x	,y	,"Depth",white,black_transparent);
		x+=w;
		w/=2;
		l/=2;
	}
}
