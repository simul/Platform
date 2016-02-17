
#ifdef _MSC_VER
#define NOMINMAX
#endif
#include "MixedResolutionView.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
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
	,Downscale(4)
	,viewType(MAIN_3D_VIEW)
	,useExternalFramebuffer(false)
	,hdrFramebuffer(NULL)
	,resolvedTexture(NULL)
	, last_framenumber(-1)
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
		hdrFramebuffer		=renderPlatform->CreateFramebuffer("hdrFramebuffer");
		resolvedTexture		=renderPlatform->CreateTexture("Resolved");	
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
	if(ScreenWidth==w&&ScreenHeight==h)
		return;
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

void MixedResolutionViewManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	auto views=GetViews();
	for(auto i=views.begin();i!=views.end();i++)
	{
		(i->second)->RestoreDeviceObjects(renderPlatform);
	}
}

void MixedResolutionViewManager::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
	auto views=GetViews();
	for(auto i=views.begin();i!=views.end();i++)
	{
		(i->second)->InvalidateDeviceObjects();
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
	if(views.find(view_id)!=views.end())
	{
		delete views[view_id];
		views.erase(view_id);
	}
}

MixedResolutionView *MixedResolutionViewManager::GetView(int view_id)
{
	ViewMap::iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

const MixedResolutionView *MixedResolutionViewManager::GetView(int view_id) const
{
	ViewMap::const_iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

const MixedResolutionViewManager::ViewMap &MixedResolutionViewManager::GetViews() const
{
	return views;
}
void MixedResolutionViewManager::CleanUp(int current_frame,int max_age)
{
	for (auto i:views)
	{
		int age=current_frame-i.second->last_framenumber;
		if(age>max_age)
		{
			RemoveView(i.first);
			break;
		}
	}
}
void MixedResolutionViewManager::Clear()
{
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
	{
		delete i->second;
	}
	views.clear();
}