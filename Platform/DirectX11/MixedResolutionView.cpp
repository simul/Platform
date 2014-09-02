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
#ifdef _XBOX_ONE
		hdrFramebuffer.SetUseESRAM(true,false);
#endif
		hdrFramebuffer.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	}
}

void MixedResolutionView::InvalidateDeviceObjects()
{
	hdrFramebuffer.InvalidateDeviceObjects();
	lowResScratch.InvalidateDeviceObjects();
	hiResScratch.InvalidateDeviceObjects();
	lowResDepthTexture.InvalidateDeviceObjects();
	hiResDepthTexture.InvalidateDeviceObjects();
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
			pContext->ResolveSubresource(resolvedTexture.texture,0,hdrFramebuffer.GetColorTexture(),0,dx11::RenderPlatform::ToDxgiFormat(crossplatform::RGBA_16_FLOAT));
			SIMUL_COMBINED_PROFILE_END(pContext)
		}
	}
}

void MixedResolutionView::RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,int x0,int y0,int dx,int dy)
{
//	int w		=lowResDepthTexture.width;
	int w=dx/2;
	int l		=(lowResDepthTexture.length*w)/lowResDepthTexture.width;
	if(l>dy/2)
	{
		l=dy/2;
		w		=(lowResDepthTexture.width*l)/lowResDepthTexture.length;
	}
	deviceContext.renderPlatform->DrawDepth(deviceContext	,x0		,y0		,w,l,depthTexture	);
	
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
	depthForwardEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	defines["REVERSE_DEPTH"]		="1";
	depthReverseEffect=renderPlatform->CreateEffect("mixed_resolution",defines);
	mixedResolutionConstants.LinkToEffect(depthForwardEffect,"MixedResolutionConstants");
	mixedResolutionConstants.LinkToEffect(depthReverseEffect,"MixedResolutionConstants");
}
#pragma optimize("",off)
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
	SIMUL_COMBINED_PROFILE_START(pContext,"DownscaleDepth")
	static int BLOCKWIDTH								=8;
	crossplatform::Effect *effect=depthForwardEffect;
	simul::camera::Frustum frustum=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	if(frustum.reverseDepth)
		effect=depthReverseEffect;
	SIMUL_ASSERT(depthTexture!=NULL);
	int W=(FullWidth+hiResDownscale-1)/hiResDownscale;
	int H=(FullHeight+hiResDownscale-1)/hiResDownscale;
	// The downscaled size should be enough to fit in at least lowResDownscale hi-res pixels in every larger pixel
	int w=(FullWidth+lowResDownscale-1)/lowResDownscale;
	int h=(FullHeight+lowResDownscale-1)/lowResDownscale;
	view->GetLowResDepthTexture()	->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,w,h,view->GetDepthFormat(),/*computable=*/true,/*rendertarget=*/false);
	bool msaa=(depthTexture->GetSampleCount()>0);
	
	static bool spread=false;
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups.
#ifdef _XBOX_ONE
	static bool use_rt=false;
#else
	static bool use_rt=true;
#endif
	{
		view->GetHiResDepthTexture()	->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,W,H,view->GetDepthFormat(),/*computable=*/!use_rt,/*rendertarget=*/use_rt,false,1,0,false);
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Hi-res Depth")
		mixedResolutionConstants.scale						=uint2(hiResDownscale,hiResDownscale);
		mixedResolutionConstants.depthToLinFadeDistParams	=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ						=0;
		mixedResolutionConstants.farZ						=0;
		mixedResolutionConstants.source_offset				=uint2(StartX,StartY);
		mixedResolutionConstants.source_dims				=uint2(depthTexture->GetWidth(),depthTexture->GetLength());
		mixedResolutionConstants.target_dims				=uint2(W,H);
		mixedResolutionConstants.Apply(deviceContext);
		uint2 subgrid						=uint2((view->hiResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(view->hiResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
		if(msaa)
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceMSDepthTexture"	,depthTexture->AsD3D11ShaderResourceView());
		else
			simul::dx11::setTexture			(effect->asD3DX11Effect(),"sourceDepthTexture"		,depthTexture->AsD3D11ShaderResourceView());
		std::string pass_name=msaa?"msaa":"main";
		if(msaa)
			pass_name+=('0'+(char)depthTexture->GetSampleCount());
	SIMUL_GPU_PROFILE_START(pContext,"Initial Downscale Far Near")
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
			//static int horizontal_block=32;
			//int block_block=horizontal_block*BLOCKWIDTH;
			//subgrid.x=(W+block_block-1)/block_block;
			pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		effect->SetUnorderedAccessView	(deviceContext,"target2DTexture"	,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"		,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture"		,NULL);
	SIMUL_GPU_PROFILE_END(pContext)
	SIMUL_GPU_PROFILE_START(pContext,"Spread")
		unbindTextures(effect->asD3DX11Effect());
	SIMUL_GPU_PROFILE_END(pContext)
		unbindTextures(effect->asD3DX11Effect());
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"cs_make_depth_far_near",pass_name.c_str());
		SIMUL_COMBINED_PROFILE_END(pContext)
	}
	{
		SIMUL_COMBINED_PROFILE_START(pContext,"Make Lo-res Depth")
		mixedResolutionConstants.scale=uint2(lowResDownscale/hiResDownscale,lowResDownscale/hiResDownscale);
		mixedResolutionConstants.depthToLinFadeDistParams=depthToLinFadeDistParams;
		mixedResolutionConstants.nearZ=0;
		mixedResolutionConstants.farZ=0;
		mixedResolutionConstants.source_dims=uint2(W,H);
		mixedResolutionConstants.target_dims=uint2(w,h);
		// if using rendertarget we must rescale the texCoords.
		//mixedResolutionConstants.mixedResolutionTransformXYWH=vec4(0.f,0.f,(float)(w*lowResDownscale)/(float)FullWidth,(float)(h*lowResDownscale)/(float)FullHeight);
		mixedResolutionConstants.Apply(deviceContext);
		uint2 subgrid						=uint2((view->GetLowResDepthTexture()->GetWidth()+BLOCKWIDTH-1)/BLOCKWIDTH,(view->GetLowResDepthTexture()->GetLength()+BLOCKWIDTH-1)/BLOCKWIDTH);
	
		SIMUL_GPU_PROFILE_START(pContext,"Downscale Hi to Low")
		{
			effect->SetTexture				(deviceContext,"sourceDepthTexture"		,view->GetHiResDepthTexture());
			crossplatform::Texture *targ=view->GetLowResDepthTexture();
			effect->SetUnorderedAccessView	(deviceContext,"target2DTexture",targ);
		simul::dx11::applyPass(pContext,effect->asD3DX11Effect(),"downscale_depth_far_near_from_hires");
		pContext->Dispatch(subgrid.x,subgrid.y,1);
		}
		SIMUL_GPU_PROFILE_END(pContext)
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceDepthTexture"		,NULL);
		simul::dx11::setTexture(effect->asD3DX11Effect(),"sourceMSDepthTexture"		,NULL);
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

void MixedResolutionViewManager::DownscaleDepth(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const crossplatform::Viewport *v,int lowResDownscale,float max_dist_metres,bool includeLowRes)
{
	MixedResolutionView *view=GetView(deviceContext.viewStruct.view_id);
	mixedResolutionRenderer.DownscaleDepth(deviceContext,depthTexture,v,view, 2,lowResDownscale
		,(const float *)simul::camera::GetDepthToDistanceParameters(deviceContext.viewStruct,max_dist_metres),includeLowRes);
}

void MixedResolutionViewManager::RecompileShaders()
{
	mixedResolutionRenderer.RecompileShaders();
}
