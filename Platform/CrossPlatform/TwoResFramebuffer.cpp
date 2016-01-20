#define NOMINMAX
#include "Simul/Platform/CrossPlatform/TwoResFramebuffer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Math/Vector3.h"
#include <algorithm>

using namespace simul;
using namespace crossplatform;
#pragma optimize("",off)

void AmortizationStruct::setAmortization(int a)
{
	if(amortization==a)
		return;
	delete [] pattern;
	pattern=NULL;
	simul::math::RandomNumberGenerator rand;
	amortization=a;
	if(a<=1)
		return;
	std::vector<int2> src;
	src.reserve(a*a);
	int n=0;
	for(int i=0;i<a;i++)
	{
		for(int j=0;j<a;j++)
		{
			int2 v(i,j);
			src.push_back(v);
			n++;
		}
	}
	pattern=new int2[n];
	for(int i=0;i<n;i++)
	{
		int idx=rand.IRand(src.size());
		auto u=src.begin()+idx;
		int2 v=*u;
		pattern[i]=v;
		src.erase(u);
	}
}

TwoResFramebuffer::TwoResFramebuffer()
	:renderPlatform(0)
	,lossTexture(NULL)
	,Width(0)
	,Height(0)
	,BufferWidth(0)
	,BufferHeight(0)
	,Downscale(0)
	,pixelOffset(0.f,0.f)
	,depthFormat(RGBA_32_FLOAT)
	,final_octave(0)
	,volume_num(0)
{
	volumeTextures[0] = volumeTextures[1] = NULL;
	for (int i = 0; i <4; i++)
		lowResFramebuffers[i] = NULL;
	for(int i=0;i<4;i++)
		nearFarTextures[i]=NULL;
	for(int i=0;i<2;i++)
		updateTextures[i]=NULL;
}

void TwoResFramebuffer::Swap()
{
	for(int i=1;i<3;i++)
	{
		std::swap(nearFarTextures[i],nearFarTextures[1+(i+1)%3]);
	}
}

void TwoResFramebuffer::SwapUpdateTextures()
{
	std::swap(updateTextures[0],updateTextures[1]);
}
 
crossplatform::Texture *TwoResFramebuffer::GetLowResDepthTexture(int idx)
{
	return nearFarTextures[idx>=0?idx:0];
}

crossplatform::Texture *TwoResFramebuffer::GetUpdateTexture(int idx)
{
	return updateTextures[idx];
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
	ERRNO_BREAK
	renderPlatform	=r;
	amortizationStruct.reset();
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(volumeTextures[0]);
	SAFE_DELETE(volumeTextures[1]);
	for (int i = 0; i < 4; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	if(!renderPlatform)
		return;
	for (int i = 0; i < 4; i++)
	{
		lowResFramebuffers[i]=renderPlatform->CreateTexture();
	}
	ERRNO_CHECK
	for(int i=0;i<4;i++)
	{
	ERRNO_CHECK
		SAFE_DELETE(nearFarTextures[i]);
	ERRNO_CHECK
		nearFarTextures[i]=renderPlatform->CreateTexture("ESRAM");
	ERRNO_CHECK
		nearFarTextures[i]->MoveToFastRAM();
	ERRNO_CHECK
	}
	for(int i=0;i<2;i++)
	{
	ERRNO_CHECK
		SAFE_DELETE(updateTextures[i]);
	ERRNO_CHECK
		updateTextures[i]=renderPlatform->CreateTexture("ESRAM");
	ERRNO_CHECK
		updateTextures[i]->MoveToFastRAM();
	ERRNO_CHECK
	}
	ERRNO_CHECK
	lossTexture			=renderPlatform->CreateTexture();
	ERRNO_CHECK
	volumeTextures[0]	=renderPlatform->CreateTexture();
	ERRNO_CHECK
	volumeTextures[1]	=renderPlatform->CreateTexture();
	ERRNO_CHECK
	if(Width<=0||Height<=0||Downscale<=0)
		return;
	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	BufferWidth = (Width + Downscale - 1) / Downscale + 1;
	BufferHeight = (Height + Downscale - 1) / Downscale + 1;
	if(Downscale==1)
	{
		BufferWidth=Width;
		BufferHeight=Height;
	}
	ERRNO_CHECK
	for (int i = 0; i < 4; i++)
	{
	//	lowResFramebuffers[i]->ensureTexture2DSizeAndFormat(renderPlatform,BufferWidth,BufferWidth,crossplatform::RGBA_16_FLOAT,true,true);
		lowResFramebuffers[i]->ensureTextureArraySizeAndFormat(renderPlatform,BufferWidth,BufferWidth,6,crossplatform::RGBA_16_FLOAT,true,false,true);
	}
	ERRNO_CHECK
	// We're going to TRY to encode near and far loss into two UINT's, for faster results
	lossTexture->ensureTexture2DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,crossplatform::RGBA_16_FLOAT,false,true);

	static bool fill_volumes_with_compute=true;
	volumeTextures[0]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,fill_volumes_with_compute,1,!fill_volumes_with_compute);
	volumeTextures[1]->ensureTexture3DSizeAndFormat(renderPlatform,BufferWidth,BufferHeight,8,simul::crossplatform::RGBA_16_FLOAT,fill_volumes_with_compute,1,!fill_volumes_with_compute);
	ERRNO_CHECK
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	for (int i = 0; i < 4; i++)
		SAFE_DELETE(lowResFramebuffers[i]);
	for(int i=0;i<4;i++)
		SAFE_DELETE(nearFarTextures[i]);
	for(int i=0;i<2;i++)
		SAFE_DELETE(updateTextures[i]);
	SAFE_DELETE(lossTexture);
	SAFE_DELETE(volumeTextures[0]);
	SAFE_DELETE(volumeTextures[1]);
	Width=0;
	Height=0;
	BufferWidth=0;
	BufferHeight=0;
}

void TwoResFramebuffer::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Texture * targs[] = { GetLowResFramebuffer(0), GetLowResFramebuffer(1), GetLowResFramebuffer(2) };
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
	crossplatform::Texture * targs[] = { GetLowResFramebuffer(0), GetLowResFramebuffer(1), GetLowResFramebuffer(2) };
///	crossplatform::Texture * depth = GetLowResFramebuffer(0)->GetDepthTexture();
	static int u = 3;
	renderPlatform->ActivateRenderTargets(deviceContext,u,targs,NULL);
}

void TwoResFramebuffer::DeactivateLowRes(crossplatform::DeviceContext &deviceContext)
{
	renderPlatform->DeactivateRenderTargets(deviceContext);
}

void TwoResFramebuffer::CompleteFrame()
{
	amortizationStruct.framenumber++;
	int D=Downscale;
	amortizationStruct.validate(int4(0,0,(Width+D-1)/D+1,(Height+D-1)/D+1));
}

void TwoResFramebuffer::ActivateVolume(crossplatform::DeviceContext &deviceContext,int num)
{
	renderPlatform->PushRenderTargets(deviceContext);
	// activate all of the rt's of this texture at once.
	volume_num=num;
	volumeTextures[num]->activateRenderTarget(deviceContext);
	int w = GetLowResFramebuffer(0)->width, h = GetLowResFramebuffer(0)->length;
	crossplatform::Viewport v[]={{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f},{0,0,w,h,0,1.f}};
	renderPlatform->SetViewports(deviceContext,volumeTextures[num]->depth,v);
}

void TwoResFramebuffer::DeactivateVolume(crossplatform::DeviceContext &deviceContext)
{
	volumeTextures[volume_num]->deactivateRenderTarget();
	renderPlatform->PopRenderTargets(deviceContext);
}

void TwoResFramebuffer::SetDimensions(int w,int h)
{
	if(Width!=w||Height!=h)
	{
		Width=w;
		Height=h;
		RestoreDeviceObjects(renderPlatform);
	}
}

void TwoResFramebuffer::SetCubeFrustumRange(int i,vec4 r)
{
	cubeFrustumRange[i]=r;
}

vec4 TwoResFramebuffer::GetCubeFrustumRange(int i) const
{
	return cubeFrustumRange[i];
}

void TwoResFramebuffer::SetDownscale(int d)
{
	if(Downscale!=d)
	{
		Downscale=d;
		RestoreDeviceObjects(renderPlatform);
	}
}

void TwoResFramebuffer::GetDimensions(int &w,int &h)
{
	w=Width;
	h=Height;
}

void TwoResFramebuffer::SetProjection(const float *p)
{
	proj=p;
}

void TwoResFramebuffer::UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct)
{
	if(Downscale<=1)
		return;
	using namespace math;
	// Update the orientation due to changing view_dir:
	Vector3 cam_pos,new_view_dir,new_view_dir_local,new_up_dir;
	simul::crossplatform::GetCameraPosVector(viewStruct.view,cam_pos,new_view_dir,new_up_dir);
	new_view_dir.Normalize();
	view_o.GlobalToLocalDirection(new_view_dir_local,new_view_dir);
	float dx			=new_view_dir*view_o.Tx();
	float dy			=new_view_dir*view_o.Ty();
	dx					*=Width*viewStruct.proj._11;
	dy					*=Height*viewStruct.proj._22;
	view_o.DefineFromYZ(new_up_dir,new_view_dir);
	static float cc=0.5f;
	vec2 dp				(-cc*dx,-cc*dy);
	vec2 oldPixelOffset	=pixelOffset;
	pixelOffset			+=dp;

	int2 sc(Downscale,Downscale);
	{
		if(sc.x<1)
			sc.x=1;
		if(sc.y<1)
			sc.y=1;
		pixelOffset.x/=(float)sc.x;
		pixelOffset.y/=(float)sc.y;
		vec2 intOffset;
		pixelOffset.x = modf (pixelOffset.x , &intOffset.x);
		if(pixelOffset.x<0.0f)
		{
			pixelOffset.x +=1.0f;
			intOffset.x	-=1.0f;
		}
		pixelOffset.y = modf (pixelOffset.y , &intOffset.y);
		if(pixelOffset.y<0.0f)
		{
			pixelOffset.y +=1.0f;
			intOffset.y	-=1.0f;
		}
	
		pixelOffset.x*=(float)sc.x;
		pixelOffset.y*=(float)sc.y;
		int2 io=int2((int)intOffset.x,(int)intOffset.y);
		
		amortizationStruct.updateRegion(io,pixelOffset/float(Downscale));
		amortizationStruct.lowResOffset+=io;

		int2 texsize(BufferWidth,BufferHeight);
		while(amortizationStruct.lowResOffset.x<0)
			amortizationStruct.lowResOffset.x+=texsize.x;
		while(amortizationStruct.lowResOffset.y<0)
			amortizationStruct.lowResOffset.y+=texsize.y;
		amortizationStruct.lowResOffset.x=amortizationStruct.lowResOffset.x%texsize.x;
		amortizationStruct.lowResOffset.y=amortizationStruct.lowResOffset.y%texsize.y;
	}
}

void TwoResFramebuffer::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const crossplatform::Viewport *viewport,int x0,int y0,int dx,int dy)
{
	vec4 white(1.0f,1.0f,1.0f,1.0f);
	vec4 black_transparent(0.0f,0.0f,0.0f,0.5f);
	int w		=dx;
	int l		=0;
	if(GetLowResDepthTexture()->width>0)
		l		=(GetLowResDepthTexture()->length*w)/GetLowResDepthTexture()->width;
	if(l==0&&depthTexture&&depthTexture->width)
	{
		l		=(depthTexture->length*w)/depthTexture->width;
	}
	if(l>dy/20)
	{
		l			=dy;
		if(GetLowResDepthTexture()->length>0)
			w		=(GetLowResDepthTexture()->width*l)/GetLowResDepthTexture()->length;
		else if(depthTexture&&depthTexture->length)
			w		=(depthTexture->width*l)/depthTexture->length;
	}
	int y=y0;
	int W=dx/3,L=l/2;
	int x=x0+W+4;
	for(int i=0;i<4;i++)
	{
		crossplatform::Texture *t=GetLowResDepthTexture(i);
		if(!t)
			continue;
		if(!i)
			deviceContext.renderPlatform->DrawTexture(deviceContext	,x		,y	,W,L,	t);
		else
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x		,y	,W,L,	t,NULL,proj);
		//deviceContext.renderPlatform->DrawTexture(deviceContext	,x+W+4	,y	,W,L,	t);
		deviceContext.renderPlatform->Print(deviceContext		,x		,y	,base::QuickFormat("Depth %d",i),white,black_transparent);
		x+=W+4;
		if(i==1)
		{
			x=x0+W+4;
			y+=L;
		}
	}
	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0	,y0		,W,L,updateTextures[0]);
	deviceContext.renderPlatform->Print(deviceContext		,x0	,y0		,"Update 0");
	deviceContext.renderPlatform->DrawTexture(deviceContext	,x0	,y0+L	,W,L,updateTextures[1]);
	deviceContext.renderPlatform->Print(deviceContext		,x0	,y0+L	,"Update 1");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0-W	,y0	,W,L,depthTexture,viewport,proj);
	deviceContext.renderPlatform->Print(deviceContext			,x0-W	,y0	,"Main Depth",white,black_transparent);
}
