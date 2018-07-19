
#ifdef _MSC_VER
#define NOMINMAX
#endif
#include "View.h"
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

int View::last_class_id=0;
int View::static_class_id=0;

View::View()
	:viewType(MAIN_3D_VIEW)
	,hdrFramebuffer(NULL)
	,resolvedTexture(NULL)
	,ScreenWidth(0)
	,ScreenHeight(0)
	,last_framenumber(-1)
	,useExternalFramebuffer(false)
 {
	if(!static_class_id)
		static_class_id=last_class_id++;
 }

 View::~View()
 {
	 InvalidateDeviceObjects();
 }

void View::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
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

void View::InvalidateDeviceObjects()
{
	if(hdrFramebuffer)
		hdrFramebuffer->InvalidateDeviceObjects();
	if(resolvedTexture)
		resolvedTexture->InvalidateDeviceObjects();
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(resolvedTexture);
}

int View::GetScreenWidth() const
{
	return ScreenWidth;
}

int View::GetScreenHeight() const
{
	return ScreenHeight;
}

void View::SetResolution(int w,int h)
{
	if(ScreenWidth==w&&ScreenHeight==h)
		return;
	ScreenWidth	=w;
	ScreenHeight=h;
	if(hdrFramebuffer)
	{
		hdrFramebuffer->SetWidthAndHeight(ScreenWidth,ScreenHeight);
	}
}

void View::SetExternalFramebuffer(bool e)
{
	if(useExternalFramebuffer!=e)
	{
		useExternalFramebuffer=e;
		hdrFramebuffer->InvalidateDeviceObjects();
	}
}

crossplatform::Texture *View::GetResolvedHDRBuffer()
{
	if(hdrFramebuffer->GetTexture()->GetSampleCount()>1)
		return resolvedTexture;
	else
		return hdrFramebuffer->GetTexture();
}

void ViewManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	//auto views=GetViews();
	for(auto i=views.begin();i!=views.end();i++)
	{
		(i->second)->RestoreDeviceObjects(renderPlatform);
	}
}

void ViewManager::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
	//auto views=GetViews();
	for(auto i=views.begin();i!=views.end();i++)
	{
		(i->second)->InvalidateDeviceObjects();
	}
}

View *ViewManager::AddView(int id)
{
	View *view=new View();
	AddView(id,view);
	return view;
}

void	ViewManager::AddView(int id,View *v)
{
	int view_id		=id;
	View *view		=views[view_id]=v;
	view->useExternalFramebuffer=false;
	view->RestoreDeviceObjects(renderPlatform);
}

void ViewManager::RemoveView(int view_id)
{ 
	if(views.find(view_id)!=views.end())
	{
		delete views[view_id];
		views.erase(view_id);
	}
}

View *ViewManager::GetView(int view_id)
{
	ViewMap::iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

const View *ViewManager::GetView(int view_id) const
{
	ViewMap::const_iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

const ViewManager::ViewMap &ViewManager::GetViews() const
{
	return views;
}
void ViewManager::CleanUp(int current_frame,int max_age)
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
void ViewManager::Clear()
{
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
	{
		delete i->second;
	}
	views.clear();
}