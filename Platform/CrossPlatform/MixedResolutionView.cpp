
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

void MixedResolutionViewManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	std::set<MixedResolutionView*> views=GetViews();
	for(std::set<MixedResolutionView*>::iterator i=views.begin();i!=views.end();i++)
	{
		(*i)->RestoreDeviceObjects(renderPlatform);
	}
}

void MixedResolutionViewManager::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
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