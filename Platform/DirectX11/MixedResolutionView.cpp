#define NOMINMAX
#include "MixedResolutionView.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Camera/Camera.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

 MixedResolutionView::MixedResolutionView()
	:camera(NULL)
	,viewType(MAIN_3D_VIEW)
	,ScreenWidth(0)
	,ScreenHeight(0)
	,useExternalFramebuffer(false)
	,externalDepthTexture(NULL)
 {
 }

 MixedResolutionView::~MixedResolutionView()
 {
	 InvalidateDeviceObjects();
 }

void MixedResolutionView::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(renderPlatform&&!useExternalFramebuffer)
	{
		hdrFramebuffer.RestoreDeviceObjects(renderPlatform);
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
	externalDepthTexture=NULL;
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

void MixedResolutionView::SetExternalDepthTexture(crossplatform::Texture *d)
{
	externalDepthTexture=d;
}

void MixedResolutionView::ResolveFramebuffer(crossplatform::DeviceContext &deviceContext)
{
	if(!useExternalFramebuffer)
	{
		if(hdrFramebuffer.numAntialiasingSamples>1)
		{
			SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"ResolveFramebuffer")
			resolvedTexture.ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,ScreenWidth,ScreenHeight,crossplatform::RGBA_16_FLOAT,false,true);
			ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
			pContext->ResolveSubresource(resolvedTexture.texture,0,hdrFramebuffer.GetColorTexture(),0,dx11::RenderPlatform::ToDxgiFormat(crossplatform::RGBA_16_FLOAT));
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
	if(externalDepthTexture)
	{
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,externalDepthTexture	);
	}
	else
		deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,hdrFramebuffer.GetDepthTexture());
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0		,"Main Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0		,y0+l	,w,l,&hiResDepthTexture	);
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0+l	,"Hi-Res Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0+w	,y0+l	,w,l,&lowResDepthTexture);
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
	depthForwardEffect=renderPlatform->CreateEffect("mixed_resolution.fx",defines);
	defines["REVERSE_DEPTH"]		="1";
	depthReverseEffect=renderPlatform->CreateEffect("mixed_resolution.fx",defines);
	mixedResolutionConstants.LinkToEffect(depthForwardEffect,"MixedResolutionConstants");
	mixedResolutionConstants.LinkToEffect(depthReverseEffect,"MixedResolutionConstants");
}

void MixedResolutionRenderer::DownscaleDepth(crossplatform::DeviceContext &deviceContext,MixedResolutionView *view,int s,vec3 depthToLinFadeDistParams)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	SIMUL_COMBINED_PROFILE_START(pContext,"DownscaleDepth")
	int W=0,H=0;
	crossplatform::Texture *depthTexture=NULL;
	if(view->useExternalFramebuffer)
	{
		if(view->externalDepthTexture)
		{
			H				=view->ScreenHeight;
			W				=view->ScreenWidth;
			depthTexture	=view->externalDepthTexture;
		}
	}
	else
	{
		W				=view->GetFramebuffer()->Width;
		H				=view->GetFramebuffer()->Height;
		depthTexture	=view->GetFramebuffer()->GetDepthTexture();
	}
	if(!W||!H)
		return;
	crossplatform::Effect *effect=depthForwardEffect;
	simul::camera::Frustum frustum=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	if(frustum.reverseDepth)
		effect=depthReverseEffect;
	SIMUL_ASSERT(depthTexture!=NULL);
	// The downscaled size should be enough to fit in at least s hi-res pixels in every larger pixel
	int w=(W+s-1)/s;
	int h=(H+s-1)/s;
	view->GetLowResDepthTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,crossplatform::RGBA_32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	view->GetLowResScratchTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,crossplatform::RGBA_32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	bool msaa=(depthTexture->GetSampleCount()>0);
	
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups.
	static bool use_rt=true;
	{
		view->GetHiResDepthTexture()->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,W,H,crossplatform::RGBA_32_FLOAT,/*computable=*/!use_rt,/*rendertarget=*/use_rt);
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Hi-res Depth")
		mixedResolutionConstants.scale		=uint2(s,s);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ		=0;
		mixedResolutionConstants.farZ		=0;
		mixedResolutionConstants.source_dims=uint2(view->hiResDepthTexture.width,view->hiResDepthTexture.length);
		mixedResolutionConstants.Apply(deviceContext);
		static int BLOCKWIDTH			=8;
		uint2 subgrid						=uint2((view->hiResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(view->hiResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
		if(msaa)
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceMSDepthTexture"	,depthTexture->AsD3D11ShaderResourceView());
		else
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceDepthTexture"					,depthTexture->AsD3D11ShaderResourceView());
		simul::dx11::setUnorderedAccessView	(effect->asD3DX11Effect(),"target2DTexture"						,view->hiResDepthTexture.unorderedAccessView);
	
		std::string pass_name=msaa?"msaa":"main";
		if(msaa)
			pass_name+=('0'+depthTexture->GetSampleCount());
		if(use_rt)
		{
			view->GetHiResDepthTexture()->activateRenderTarget(deviceContext);
			simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"make_depth_far_near",pass_name.c_str());
			UtilityRenderer::DrawQuad(pContext);
			view->GetHiResDepthTexture()->deactivateRenderTarget();
		}
		else
		{
			simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"cs_make_depth_far_near",pass_name.c_str());
			pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		unbindTextures(effect->asD3DX11Effect());
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"cs_make_depth_far_near",pass_name.c_str());
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Lo-res Depth")
		mixedResolutionConstants.scale=uint2(s,s);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ=0;
		mixedResolutionConstants.farZ=0;
		mixedResolutionConstants.source_dims=uint2(view->GetHiResDepthTexture()->GetWidth(),view->GetHiResDepthTexture()->GetLength());
		// if using rendertarget we must rescale the texCoords.
		//mixedResolutionConstants.mixedResolutionTransformXYWH=vec4(0.f,0.f,(float)(w*s)/(float)W,(float)(h*s)/(float)H);
		mixedResolutionConstants.Apply(deviceContext);
		static int BLOCKWIDTH				=8;
		uint2 subgrid						=uint2((view->GetLowResDepthTexture()->GetWidth()+BLOCKWIDTH-1)/BLOCKWIDTH,(view->GetLowResDepthTexture()->GetLength()+BLOCKWIDTH-1)/BLOCKWIDTH);
		if(msaa)
			simul::dx11::setTexture				(effect->asD3DX11Effect(),"sourceMSDepthTexture"	,depthTexture->AsD3D11ShaderResourceView());
		simul::dx11::setTexture				(effect->asD3DX11Effect(),"sourceDepthTexture"		,view->GetHiResDepthTexture()->AsD3D11ShaderResourceView());
		simul::dx11::setUnorderedAccessView	(effect->asD3DX11Effect(),"target2DTexture"		,((dx11::Texture *)view->GetLowResScratchTexture())->unorderedAccessView);
	
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
		pContext->Dispatch(subgrid.x,subgrid.y,1);
		simul::dx11::setTexture				(effect->asD3DX11Effect(),"sourceDepthTexture"		,view->GetLowResScratchTexture()->AsD3D11ShaderResourceView());
		simul::dx11::setUnorderedAccessView	(effect->asD3DX11Effect(),"target2DTexture"		,((dx11::Texture *)view->GetLowResDepthTexture())->unorderedAccessView);
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"spread_edge");
		pContext->Dispatch(subgrid.x,subgrid.y,1);
		unbindTextures(effect->asD3DX11Effect());
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
		SIMUL_COMBINED_PROFILE_END(pContext)
			
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture",NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"	,NULL);
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
	}
	SIMUL_COMBINED_PROFILE_END(pContext)
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

void MixedResolutionViewManager::DownscaleDepth(crossplatform::DeviceContext &deviceContext,int s,float max_dist_metres)
{
	MixedResolutionView *view=GetView(deviceContext.viewStruct.view_id);
	mixedResolutionRenderer.DownscaleDepth(deviceContext,view,s,(const float *)simul::camera::GetDepthToDistanceParameters((const float*)&deviceContext.viewStruct.proj,max_dist_metres));
}

void MixedResolutionViewManager::RecompileShaders()
{
	mixedResolutionRenderer.RecompileShaders();
}