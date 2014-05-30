#define NOMINMAX
#include "View.h"
#include "Simul/Base/ProfilingInterface.h"

using namespace simul;
using namespace dx11;

 View::View()
	:camera(NULL)
	,viewType(MAIN_3D_VIEW)
	,ScreenWidth(0)
	,ScreenHeight(0)
	,useExternalFramebuffer(false)
	,externalDepthTexture_SRV(NULL)
 {
 }

 View::~View()
 {
	 InvalidateDeviceObjects();
 }

void View::RestoreDeviceObjects(ID3D11Device *pd3dDevice)
{
	m_pd3dDevice=pd3dDevice;
	if(!useExternalFramebuffer)
	{
		hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
		hdrFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	}
}

void View::InvalidateDeviceObjects()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	lowResScratch.InvalidateDeviceObjects();
	lowResDepthTexture.InvalidateDeviceObjects();
	hiResDepthTexture.InvalidateDeviceObjects();
	resolvedTexture.InvalidateDeviceObjects();
	SAFE_RELEASE(externalDepthTexture_SRV);
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
	ScreenWidth	=w;
	ScreenHeight=h;
}

void View::SetExternalFramebuffer(bool e)
{
	if(useExternalFramebuffer!=e)
	{
		useExternalFramebuffer=e;
		hdrFramebuffer.InvalidateDeviceObjects();
	}
}

void View::SetExternalDepthResource(ID3D11ShaderResourceView *d)
{
	externalDepthTexture_SRV=d;
}

void View::ResolveFramebuffer(ID3D11DeviceContext *pContext)
{
	if(!useExternalFramebuffer)
	{
		if(hdrFramebuffer.numAntialiasingSamples>1)
		{
			SIMUL_COMBINED_PROFILE_START(pContext,"ResolveFramebuffer")
			resolvedTexture.ensureTexture2DSizeAndFormat(m_pd3dDevice,ScreenWidth,ScreenHeight,DXGI_FORMAT_R16G16B16A16_FLOAT,false,true);
			pContext->ResolveSubresource(resolvedTexture.texture,0,hdrFramebuffer.GetColorTexture(),0,DXGI_FORMAT_R16G16B16A16_FLOAT);
			SIMUL_COMBINED_PROFILE_END(pContext)
		}
	}
}

ID3D11ShaderResourceView *View::GetResolvedHDRBuffer()
{
	if(hdrFramebuffer.numAntialiasingSamples>1)
		return resolvedTexture.shaderResourceView;
	else
		return (ID3D11ShaderResourceView*)hdrFramebuffer.GetColorTex();
}

int	ViewManager::AddView(bool external_framebuffer)
{
	last_created_view_id++;
	int view_id		=last_created_view_id;
	View *view		=views[view_id]=new View();
	view->useExternalFramebuffer=external_framebuffer;
	return view_id;
}

void ViewManager::RemoveView(int view_id)
{
	delete views[view_id];
	views.erase(view_id);
}

View *ViewManager::GetView(int view_id)
{
	ViewMap::iterator i=views.find(view_id);
	if(i==views.end())
		return NULL;
	return i->second;
}

void ViewManager::Clear()
{
	for(ViewMap::iterator i=views.begin();i!=views.end();i++)
	{
		delete i->second;
	}
	views.clear();
}