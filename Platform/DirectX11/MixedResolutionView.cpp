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
	,useExternalFramebuffer(false)
	,hiResDepthTexture(NULL)
 {
 }

 MixedResolutionView::~MixedResolutionView()
 {
	 InvalidateDeviceObjects();
 }

void MixedResolutionView::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	SAFE_DELETE(hiResDepthTexture);
	if(renderPlatform)
	{
		hiResDepthTexture=renderPlatform->CreateTexture("ESRAM");
		hiResDepthTexture->MoveToFastRAM();
		if(!useExternalFramebuffer)
		{
			hdrFramebuffer.RestoreDeviceObjects(renderPlatform);

			hdrFramebuffer.SetUseFastRAM(true,true);

			hdrFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
			hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
		}
	}
}

void MixedResolutionView::InvalidateDeviceObjects()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	lowResScratch.InvalidateDeviceObjects();
	hiResScratch.InvalidateDeviceObjects();
	lowResDepthTexture.InvalidateDeviceObjects();
	if(hiResDepthTexture)
		hiResDepthTexture->InvalidateDeviceObjects();
	SAFE_DELETE(hiResDepthTexture);
	resolvedTexture.InvalidateDeviceObjects();
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

void MixedResolutionView::ResolveFramebuffer(crossplatform::DeviceContext &deviceContext)
{
	if(!useExternalFramebuffer)
	{
		if(hdrFramebuffer.numAntialiasingSamples>1)
		{
			SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"ResolveFramebuffer")
			resolvedTexture.ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,ScreenWidth,ScreenHeight,crossplatform::RGBA_16_FLOAT,false,true);
			ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.platform_context;
			pContext->ResolveSubresource(resolvedTexture.AsD3D11Texture2D(),0,hdrFramebuffer.GetColorTexture(),0,dx11::RenderPlatform::ToDxgiFormat(crossplatform::RGBA_16_FLOAT));
			SIMUL_COMBINED_PROFILE_END(pContext)
		}
	}
}

void MixedResolutionView::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,crossplatform::Viewport *viewport,int x0,int y0,int dx,int dy)
{
	int w		=dx/2;
	int l		=0;
	if(lowResDepthTexture.width>0)
		l		=(lowResDepthTexture.length*w)/lowResDepthTexture.width;
	if(l==0&&depthTexture&&depthTexture->width)
	{
		l		=(depthTexture->length*w)/depthTexture->width;
	}
	if(l>dy/20)
	{
		l		=dy/2;
		if(lowResDepthTexture.length>0)
			w		=(lowResDepthTexture.width*l)/lowResDepthTexture.length;
		else if(depthTexture&&depthTexture->length)
			w		=(depthTexture->width*l)/depthTexture->length;
	}
	deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,depthTexture,viewport);
	
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0		,"Main Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0		,y0+l	,w,l,GetHiResDepthTexture()	);
	deviceContext.renderPlatform->Print(deviceContext			,x0		,y0+l	,"Hi-Res Depth");
	deviceContext.renderPlatform->DrawDepth(deviceContext		,x0+w	,y0+l	,w,l,&lowResDepthTexture);
	deviceContext.renderPlatform->Print(deviceContext			,x0+w	,y0+l	,"Lo-Res Depth");
}

ID3D11ShaderResourceView *MixedResolutionView::GetResolvedHDRBuffer()
{
	if(hdrFramebuffer.numAntialiasingSamples>1)
		return resolvedTexture.AsD3D11ShaderResourceView();
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
	depthForwardEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	defines["REVERSE_DEPTH"]		="1";
	depthReverseEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	mixedResolutionConstants.LinkToEffect(depthForwardEffect,"MixedResolutionConstants");
	mixedResolutionConstants.LinkToEffect(depthReverseEffect,"MixedResolutionConstants");
}

void MixedResolutionRenderer::DownscaleDepth(crossplatform::DeviceContext &deviceContext
											,crossplatform::Texture *depthTexture
											,const crossplatform::Viewport *simulViewport
											,MixedResolutionView *view
											,int hiResDownscale
											,int lowResDownscale
											,vec4 depthToLinFadeDistParams,bool includeLowResDepth)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
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
	view->UpdatePixelOffset(deviceContext.viewStruct,lowResDownscale);
	uint2 intOffset			=uint2((int)view->pixelOffset.x,(int)view->pixelOffset.y);
	vec2 fracOffset			=vec2(view->pixelOffset.x-intOffset.x,view->pixelOffset.y-intOffset.y);
	intOffset				=uint2(intOffset.x%lowResDownscale,intOffset.y%lowResDownscale);
	int scaleRatio			=lowResDownscale/hiResDownscale;
	view->pixelOffset		=vec2(intOffset.x+fracOffset.x,intOffset.y+fracOffset.y);
	vec2 hiResPixelOffset	=crossplatform::MixedResolutionView::LoResToHiResOffset(view->pixelOffset,hiResDownscale);
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"DownscaleDepth")
	static int BLOCKWIDTH	=8;
	crossplatform::Effect *effect=depthForwardEffect;
	simul::camera::Frustum frustum=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	if(frustum.reverseDepth)
		effect=depthReverseEffect;
	SIMUL_ASSERT(depthTexture!=NULL);
	int W=(FullWidth+hiResDownscale-1)	/hiResDownscale+1;
	int H=(FullHeight+hiResDownscale-1)	/hiResDownscale+1;
	// The downscaled size should be enough to fit in at least lowResDownscale hi-res pixels in every larger pixel
	int w=(FullWidth+lowResDownscale-1)	/lowResDownscale+1;
	int h=(FullHeight+lowResDownscale-1)/lowResDownscale+1;
	if(includeLowResDepth)
	{
		view->GetLowResDepthTexture()	->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,view->GetDepthFormat(),/*computable=*/true,/*rendertarget=*/false);
	}
	else
	{
		view->GetLowResDepthTexture()	->InvalidateDeviceObjects();
	}
	bool msaa=(depthTexture->GetSampleCount()>0);
	
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups.
	static bool use_rt=true;
	{
		view->GetHiResDepthTexture()	->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,W,H,view->GetDepthFormat(),/*computable=*/true,/*rendertarget=*/true);
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Make Hi-res Depth")
		mixedResolutionConstants.scale						=uint2(hiResDownscale,hiResDownscale);
		mixedResolutionConstants.depthToLinFadeDistParams	=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ						=0;
		mixedResolutionConstants.farZ						=0;
		mixedResolutionConstants.source_dims				=uint2(depthTexture->GetWidth(),depthTexture->GetLength());
		mixedResolutionConstants.target_dims				=uint2(W,H);
		mixedResolutionConstants.source_offset				=uint2(StartX,StartY);
		mixedResolutionConstants.cornerOffset				=uint2((int)hiResPixelOffset.x,(int)hiResPixelOffset.y);
		mixedResolutionConstants.Apply(deviceContext);
		uint2 subgrid						=uint2((view->GetHiResDepthTexture()->width+BLOCKWIDTH-1)/BLOCKWIDTH,(view->GetHiResDepthTexture()->length+BLOCKWIDTH-1)/BLOCKWIDTH);
		if(msaa)
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceMSDepthTexture"	,depthTexture->AsD3D11ShaderResourceView());
		else
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceDepthTexture"		,depthTexture->AsD3D11ShaderResourceView());
		std::string pass_name=msaa?"msaa":"main";
		if(msaa)
			pass_name+=('0'+depthTexture->GetSampleCount());
		else if(!use_rt&&(hiResDownscale==2||hiResDownscale==4))
			pass_name+=('0'+hiResDownscale);
		if(use_rt)
		{
			crossplatform::Texture *targ=view->GetHiResDepthTexture();
			targ->activateRenderTarget(deviceContext);
			simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near",pass_name.c_str());
			renderPlatform->DrawQuad(deviceContext);
			targ->deactivateRenderTarget();
		}
		else
		{
			effect->SetUnorderedAccessView	(deviceContext,"target2DTexture"	,view->GetHiResDepthTexture());
			simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"cs_downscale_depth_far_near",pass_name.c_str());
			pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		effect->SetUnorderedAccessView	(deviceContext,"target2DTexture"	,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"		,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture"		,NULL);
		unbindTextures(effect->asD3DX11Effect());
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"cs_downscale_depth_far_near",pass_name.c_str());
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	}
	if(includeLowResDepth)
	{
		SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Make Lo-res Depth")
		mixedResolutionConstants.scale			=uint2(scaleRatio,scaleRatio);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ=0;
		mixedResolutionConstants.farZ=0;
		mixedResolutionConstants.source_dims=uint2(W,H);
		mixedResolutionConstants.target_dims=uint2(w,h);
		mixedResolutionConstants.source_offset	=uint2(0,0);
		mixedResolutionConstants.cornerOffset	=uint2((view->pixelOffset.x-hiResPixelOffset.x)/hiResDownscale,(view->pixelOffset.y-hiResPixelOffset.y)/hiResDownscale);
		// if using rendertarget we must rescale the texCoords.
		mixedResolutionConstants.Apply(deviceContext);
		uint2 subgrid						=uint2((view->GetLowResDepthTexture()->GetWidth()+BLOCKWIDTH-1)/BLOCKWIDTH,(view->GetLowResDepthTexture()->GetLength()+BLOCKWIDTH-1)/BLOCKWIDTH);
		{
			effect->SetTexture				(deviceContext,"sourceDepthTexture"		,view->GetHiResDepthTexture());
			crossplatform::Texture *targ=view->GetLowResDepthTexture();
			effect->SetUnorderedAccessView	(deviceContext,"target2DTexture",targ);
			simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
			pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"		,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture"		,NULL);
		unbindTextures(effect->asD3DX11Effect());
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
		SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
			
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture",NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"	,NULL);
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
	}
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

void MixedResolutionViewManager::DownscaleDepth(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture
												,const crossplatform::Viewport *v,int hiResDownscale,int lowResDownscale
												,float max_dist_metres,bool includeLowRes)
{
	MixedResolutionView *view=GetView(deviceContext.viewStruct.view_id);
	mixedResolutionRenderer.DownscaleDepth(deviceContext,depthTexture,v,view, hiResDownscale,lowResDownscale
		,(const float *)simul::camera::GetDepthToDistanceParameters(deviceContext.viewStruct,max_dist_metres),includeLowRes);
}

void MixedResolutionViewManager::RecompileShaders()
{
	mixedResolutionRenderer.RecompileShaders();
}
