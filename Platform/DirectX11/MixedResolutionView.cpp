#define NOMINMAX
#include "MixedResolutionView.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

 MixedResolutionView::MixedResolutionView()
	:camera(NULL)
	,viewType(MAIN_3D_VIEW)
	,ScreenWidth(0)
	,ScreenHeight(0)
	,useExternalFramebuffer(false)
	,externalDepthTexture_SRV(NULL)
 {
 }

 MixedResolutionView::~MixedResolutionView()
 {
	 InvalidateDeviceObjects();
 }

void MixedResolutionView::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!useExternalFramebuffer)
	{
		hdrFramebuffer.RestoreDeviceObjects(renderPlatform->AsD3D11Device());
		hdrFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	}
}

void MixedResolutionView::InvalidateDeviceObjects()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	lowResScratch.InvalidateDeviceObjects();
	lowResDepthTexture.InvalidateDeviceObjects();
	hiResDepthTexture.InvalidateDeviceObjects();
	resolvedTexture.InvalidateDeviceObjects();
	SAFE_RELEASE(externalDepthTexture_SRV);
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
		hdrFramebuffer.InvalidateDeviceObjects();
	}
}

void MixedResolutionView::SetExternalDepthResource(ID3D11ShaderResourceView *d)
{
	externalDepthTexture_SRV=d;
}

void MixedResolutionView::ResolveFramebuffer(crossplatform::DeviceContext &deviceContext)
{
	if(!useExternalFramebuffer)
	{
		if(hdrFramebuffer.numAntialiasingSamples>1)
		{
			SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"ResolveFramebuffer")
			resolvedTexture.ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,ScreenWidth,ScreenHeight,DXGI_FORMAT_R16G16B16A16_FLOAT,false,true);
			ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
			pContext->ResolveSubresource(resolvedTexture.texture,0,hdrFramebuffer.GetColorTexture(),0,DXGI_FORMAT_R16G16B16A16_FLOAT);
			SIMUL_COMBINED_PROFILE_END(pContext)
		}
	}
}

void MixedResolutionView::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy)
{
	int w		=lowResDepthTexture.width*4;
	int l		=lowResDepthTexture.length*4;
	if(w>dx/2)
	{
		l*=dx/2;
		l/=w;
		w=dx/2;
		dy=(dx*l)/w;
	}
	if(l>dy/2)
	{
		w*=dy/2;
		w/=l;
		l=dy/2;
		dx=(dy*w)/l;
	}
	static float uu=10000.0f;
	if(externalDepthTexture_SRV)
	{
		dx11::Texture tex;
		tex.shaderResourceView=externalDepthTexture_SRV;
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,&tex	,deviceContext.viewStruct.proj	);
	}
	else
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,hdrFramebuffer.GetDepthTexture()	,deviceContext.viewStruct.proj	);
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0		,"Main Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0		,y0+l	,w,l,&hiResDepthTexture	,deviceContext.viewStruct.proj);
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0+l	,"Hi-Res Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0+w	,y0+l	,w,l,&lowResDepthTexture,deviceContext.viewStruct.proj);
	deviceContext.renderPlatform->Print(deviceContext			,x0+w	,y0+l	,"Lo-Res Depth");
}

ID3D11ShaderResourceView *MixedResolutionView::GetResolvedHDRBuffer()
{
	if(hdrFramebuffer.numAntialiasingSamples>1)
		return resolvedTexture.shaderResourceView;
	else
		return (ID3D11ShaderResourceView*)hdrFramebuffer.GetColorTex();
}

MixedResolutionRenderer::MixedResolutionRenderer()
		:mixedResolutionEffect(NULL)
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
	mixedResolutionConstants.RestoreDeviceObjects(renderPlatform->AsD3D11Device());
}

void MixedResolutionRenderer::InvalidateDeviceObjects()
{
	renderPlatform=NULL;
	mixedResolutionConstants.InvalidateDeviceObjects();
	SAFE_RELEASE(mixedResolutionEffect);
}

void MixedResolutionRenderer::RecompileShaders(const std::map<std::string,std::string> &defines)
{
	SAFE_RELEASE(mixedResolutionEffect);
	if(!renderPlatform)
		return;
	HRESULT hr=CreateEffect(renderPlatform->AsD3D11Device(),&mixedResolutionEffect,"mixed_resolution.fx",defines);
	mixedResolutionConstants.LinkToEffect(mixedResolutionEffect,"MixedResolutionConstants");
}

void MixedResolutionViewManager::RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform)
{
	mixedResolutionRenderer.RestoreDeviceObjects(renderPlatform);
	std::set<MixedResolutionView*> views=GetViews();
	for(std::set<MixedResolutionView*>::iterator i=views.begin();i!=views.end();i++)
	{
		(*i)->RestoreDeviceObjects(renderPlatform);
	}
}

void MixedResolutionViewManager::InvalidateDeviceObjects()
{
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

void MixedResolutionViewManager::DownscaleDepth(crossplatform::DeviceContext &deviceContext,int s,float max_dist_metres)
{
	MixedResolutionView *view=GetView(deviceContext.viewStruct.view_id);
	mixedResolutionRenderer.DownscaleDepth(deviceContext,view,s,(const float *)simul::camera::GetDepthToDistanceParameters((const float*)&deviceContext.viewStruct.proj,max_dist_metres));
}

void MixedResolutionViewManager::RecompileShaders(std::map<std::string,std::string> defines)
{
	mixedResolutionRenderer.RecompileShaders(defines);
}