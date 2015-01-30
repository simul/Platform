
#ifdef _MSC_VER
#define NOMINMAX
#endif
#include "MixedResolutionView.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/TwoResFramebuffer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Base/RuntimeError.h"
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <algorithm>
using namespace simul;
using namespace crossplatform;

MixedResolutionView::MixedResolutionView()
	:ScreenWidth(0)
	,ScreenHeight(0)
	,viewType(MAIN_3D_VIEW)
	,useExternalFramebuffer(false)
	,hdrFramebuffer(NULL)
	,resolvedTexture(NULL)
 {
 }

 MixedResolutionView::~MixedResolutionView()
 {
	 InvalidateDeviceObjects();
 }

void MixedResolutionView::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(resolvedTexture);
	if(renderPlatform)
	{
		hdrFramebuffer		=renderPlatform->CreateFramebuffer();
		resolvedTexture		=renderPlatform->CreateTexture();	
		if(!useExternalFramebuffer)
		{
			hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
			hdrFramebuffer->SetUseFastRAM(true,true);
			hdrFramebuffer->SetFormat(RGBA_16_FLOAT);
			hdrFramebuffer->SetDepthFormat(D_32_FLOAT);
		}
	}
}

void MixedResolutionView::InvalidateDeviceObjects()
{
	if(hdrFramebuffer)
		hdrFramebuffer->InvalidateDeviceObjects();
	if(resolvedTexture)
		resolvedTexture->InvalidateDeviceObjects();
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(resolvedTexture);
}

int MixedResolutionView::GetScreenWidth() const
{
	return ScreenWidth;
}

int MixedResolutionView::GetScreenHeight() const
{
	return ScreenHeight;
}

void MixedResolutionView::SetResolution(int w,int h)
{
	ScreenWidth	=w;
	ScreenHeight=h;
}

void MixedResolutionView::SetExternalFramebuffer(bool e)
{
	if(useExternalFramebuffer!=e)
	{
		useExternalFramebuffer=e;
		hdrFramebuffer->InvalidateDeviceObjects();
	}
}

crossplatform::Texture *MixedResolutionView::GetResolvedHDRBuffer()
{
	if(hdrFramebuffer->numAntialiasingSamples>1)
		return resolvedTexture;
	else
		return hdrFramebuffer->GetTexture();
}

MixedResolutionRenderer::MixedResolutionRenderer()
		:depthForwardEffect(NULL)
		,depthReverseEffect(NULL)
		,renderPlatform(NULL)
{
}

MixedResolutionRenderer::~MixedResolutionRenderer()
{
	InvalidateDeviceObjects();
}

void MixedResolutionRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	mixedResolutionConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}

void MixedResolutionRenderer::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
	mixedResolutionConstants.InvalidateDeviceObjects();
	SAFE_DELETE(depthForwardEffect);
	SAFE_DELETE(depthReverseEffect);
}

void MixedResolutionRenderer::RecompileShaders()
{
	SAFE_DELETE(depthForwardEffect);
	SAFE_DELETE(depthReverseEffect);
	if(!renderPlatform)
		return;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]		="0";
	depthForwardEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	defines["REVERSE_DEPTH"]		="1";
	depthReverseEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	mixedResolutionConstants.LinkToEffect(depthForwardEffect,"MixedResolutionConstants");
	mixedResolutionConstants.LinkToEffect(depthReverseEffect,"MixedResolutionConstants");
}

void MixedResolutionRenderer::DownscaleDepth(crossplatform::DeviceContext &deviceContext
											,crossplatform::Texture *depthTexture
											,const crossplatform::Viewport *simulViewport
											,crossplatform::TwoResFramebuffer *fb
											,int downscale
											,float max_dist_metres)
{
	vec4 depthToLinFadeDistParams=simul::crossplatform::GetDepthToDistanceParameters(deviceContext.viewStruct,max_dist_metres);
	int FullWidth=0,FullHeight=0;
	int StartX=0,StartY=0;
	if(!depthTexture)
		return;
	if(simulViewport)
	{
		FullHeight		=simulViewport->h;
		FullWidth		=simulViewport->w;
		StartX			=simulViewport->x;
		StartY			=simulViewport->y;
	}
	else
	{
		FullHeight		=depthTexture->length;
		FullWidth		=depthTexture->width;
	}
	if(!FullWidth||!FullHeight)
		return;
	fb->UpdatePixelOffset(deviceContext.viewStruct,downscale);
	uint2 intOffset			=uint2((int)fb->pixelOffset.x			,(int)fb->pixelOffset.y);
	vec2 fracOffset			=vec2(fb->pixelOffset.x-intOffset.x	,fb->pixelOffset.y-intOffset.y);
	intOffset				=uint2(intOffset.x%downscale			,intOffset.y%downscale);
	fb->pixelOffset		=vec2(intOffset.x+fracOffset.x			,intOffset.y+fracOffset.y);
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"DownscaleDepth")
	static int BLOCKWIDTH	=8;
	crossplatform::Effect *effect			=depthForwardEffect;
	simul::crossplatform::Frustum frustum	=crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	if(frustum.reverseDepth)
		effect=depthReverseEffect;
	if(!effect)
		return;
	SIMUL_ASSERT(depthTexture!=NULL);
	int W=(FullWidth+downscale-1)	/downscale+1;
	int H=(FullHeight+downscale-1)	/downscale+1;
	bool msaa=(depthTexture->GetSampleCount()>0);
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups.
	// intermediate textures: whatever factor of 2 gives downscale, subtract 1 to get the number of intermediates.
	fb->final_octave=0;
	while(1<<fb->final_octave<downscale)
		fb->final_octave++;
	for(int i=0;i<fb->final_octave;i++)
	{
		int w=W*(1<<(fb->final_octave-i-1));
		int h=H*(1<<(fb->final_octave-i-1));
		fb->GetLowResDepthTexture(i)					->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,fb->GetDepthFormat(),/*computable=*/true,/*rendertarget=*/true);
	}
	mixedResolutionConstants.scale						=uint2(downscale,downscale);
	mixedResolutionConstants.depthToLinFadeDistParams	=depthToLinFadeDistParams;
	mixedResolutionConstants.nearZ						=0;
	mixedResolutionConstants.farZ						=0;
	mixedResolutionConstants.source_dims				=uint2(depthTexture->GetWidth(),depthTexture->GetLength());
	mixedResolutionConstants.target_dims				=uint2(W,H);
	mixedResolutionConstants.source_offset				=uint2(StartX,StartY);
	mixedResolutionConstants.cornerOffset				=uint2((int)fb->pixelOffset.x,(int)fb->pixelOffset.y);
	mixedResolutionConstants.Apply(deviceContext);
	if(msaa)
		effect->SetTexture			(deviceContext,"sourceMSDepthTexture"	,depthTexture);
	else
		effect->SetTexture			(deviceContext,"sourceDepthTexture"		,depthTexture);
	std::string pass_name=msaa?"msaa":"main";
	if(msaa)
		pass_name+=(char)((int)'0'+depthTexture->GetSampleCount());
	
	crossplatform::Texture *targetTexture				=fb->GetLowResDepthTexture(0);
	targetTexture->activateRenderTarget(deviceContext);
	mixedResolutionConstants.source_dims				=uint2(depthTexture->GetWidth(),depthTexture->GetLength());
	mixedResolutionConstants.target_dims				=uint2(targetTexture->width,targetTexture->length);
	mixedResolutionConstants.scale						=uint2(2,2);
	mixedResolutionConstants.Apply(deviceContext);
	effect->Apply(deviceContext,effect->GetTechniqueByName("halfscale_initial"),pass_name.c_str());
	renderPlatform->DrawQuad(deviceContext);
	targetTexture->deactivateRenderTarget();
	effect->Unapply(deviceContext);
	for(int i=1;i<fb->final_octave;i++)
	{
		crossplatform::Texture *sourceTexture				=fb->GetLowResDepthTexture(i-1);
		targetTexture				=fb->GetLowResDepthTexture(i);
		// downscale to half:
		effect->SetTexture			(deviceContext,"sourceDepthTexture"		,sourceTexture);
		targetTexture->activateRenderTarget(deviceContext);
		// No source offset beause only the input depth texture might use a viewport.
		mixedResolutionConstants.source_offset				=uint2(0,0);
		// And we've already applied the pixel offset in the initial step:
		mixedResolutionConstants.cornerOffset				=uint2(0,0);
		mixedResolutionConstants.source_dims				=uint2(sourceTexture->GetWidth(),sourceTexture->GetLength());
		mixedResolutionConstants.target_dims				=uint2(targetTexture->width,targetTexture->length);
		mixedResolutionConstants.Apply(deviceContext);
		effect->Apply(deviceContext,effect->GetTechniqueByName("halfscale"),0);
		renderPlatform->DrawQuad(deviceContext);
		targetTexture->deactivateRenderTarget();
		effect->Unapply(deviceContext);
	}
	
	effect->UnbindTextures(deviceContext);
	effect->Apply(deviceContext,effect->GetTechniqueByName("cs_downscale_depth_far_near"),pass_name.c_str());
	effect->Unapply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
}

void MixedResolutionViewManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	mixedResolutionRenderer.RestoreDeviceObjects(renderPlatform);
	std::set<MixedResolutionView*> views=GetViews();
	for(std::set<MixedResolutionView*>::iterator i=views.begin();i!=views.end();i++)
	{
		(*i)->RestoreDeviceObjects(renderPlatform);
	}
}

void MixedResolutionViewManager::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
	mixedResolutionRenderer.InvalidateDeviceObjects();
	std::set<MixedResolutionView*> views=GetViews();
	for(std::set<MixedResolutionView*>::iterator i=views.begin();i!=views.end();i++)
	{
		(*i)->InvalidateDeviceObjects();
	}
}

int	MixedResolutionViewManager::AddView(bool external_framebuffer)
{
	last_created_view_id++;
	int view_id		=last_created_view_id;
	MixedResolutionView *view		=views[view_id]=new MixedResolutionView();
	view->useExternalFramebuffer=external_framebuffer;
	view->RestoreDeviceObjects(renderPlatform);
	return view_id;
}

void MixedResolutionViewManager::RemoveView(int view_id)
{
	delete views[view_id];
	views.erase(view_id);
}

MixedResolutionView *MixedResolutionViewManager::GetView(int view_id)
{
	ViewMap::iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

std::set<MixedResolutionView*> MixedResolutionViewManager::GetViews()
{
	std::set<MixedResolutionView*> v;
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
		v.insert(i->second);
	return v;
}

void MixedResolutionViewManager::Clear()
{
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
	{
		delete i->second;
	}
	views.clear();
}