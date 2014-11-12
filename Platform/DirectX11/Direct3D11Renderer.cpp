#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/Crossplatform/Material.h"
#include "Simul/Platform/Crossplatform/DemoOverlay.h"
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Terrain/BaseSeaRenderer.h"
#include "Simul/Platform/CrossPlatform/HDRRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Platform/CrossPlatform/BaseOpticsRenderer.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/pi.h"
#ifdef SIMUL_USE_SCENE
#include "Simul/Scene/BaseSceneRenderer.h"
#endif
using namespace simul;
using namespace dx11;

TrueSkyRenderer::TrueSkyRenderer(simul::clouds::Environment *env,simul::scene::Scene *sc,simul::base::MemoryInterface *m)
		:clouds::TrueSkyRenderer(env,sc,m)
		,ShowWaterTextures(false)
		,ShowWater(true)
		,oceanRenderer(NULL)
{
	sc;
	ReverseDepthChanged();
}

TrueSkyRenderer::~TrueSkyRenderer()
{
	InvalidateDeviceObjects();
	del(oceanRenderer,memoryInterface);
}

void TrueSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	clouds::TrueSkyRenderer::RestoreDeviceObjects(r);
	renderPlatform=r;
	if(!renderPlatform)
		return;
	if(!oceanRenderer&&simulWeatherRenderer&&(simul::base::GetFeatureLevel()&simul::base::EXPERIMENTAL)!=0)
	{
		oceanRenderer			=new(memoryInterface) terrain::BaseSeaRenderer(simulWeatherRenderer->GetEnvironment()->seaKeyframer);
		oceanRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetEnvironment()->skyKeyframer);
	}
	if(oceanRenderer)
		oceanRenderer->RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
}
// The elements in the main colour/depth buffer, with depth test and optional MSAA
void TrueSkyRenderer::RenderDepthElements(crossplatform::DeviceContext &deviceContext
									 ,float exposure
									 ,float gamma)
{
	clouds::TrueSkyRenderer::RenderDepthElements(deviceContext
									 ,exposure
									 ,gamma);
	if(oceanRenderer&&ShowWater&&(simul::base::GetFeatureLevel()&simul::base::EXPERIMENTAL)!=0)
	{
		oceanRenderer->SetMatrices(deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		oceanRenderer->Render(deviceContext,1.f);
		if(oceanRenderer->GetShowWireframes())
			oceanRenderer->RenderWireframe(deviceContext);
	}
}


void TrueSkyRenderer::InvalidateDeviceObjects()
{
	if(!renderPlatform)
		return;
	Profiler::GetGlobalProfiler().Uninitialize();
	if(oceanRenderer)
		oceanRenderer->InvalidateDeviceObjects();
	clouds::TrueSkyRenderer::InvalidateDeviceObjects();
	renderPlatform=NULL;
}

void TrueSkyRenderer::RecompileShaders()
{
	clouds::TrueSkyRenderer::RecompileShaders();
	if(oceanRenderer)
		oceanRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
}

void TrueSkyRenderer::ReverseDepthChanged()
{
	if(renderPlatform)
		renderPlatform->SetReverseDepth(ReverseDepth);
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulHDRRenderer)
		simulHDRRenderer->SetReverseDepth(ReverseDepth);
	if(baseTerrainRenderer)
		baseTerrainRenderer->SetReverseDepth(ReverseDepth);
	if(oceanRenderer)
		oceanRenderer->SetReverseDepth(ReverseDepth);
	RecompileShaders();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::scene::Scene *s,simul::base::MemoryInterface *m)
	:trueSkyRenderer(env,s,m)
{
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	trueSkyRenderer.InvalidateDeviceObjects();
}

void Direct3D11Renderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	renderPlatformDx11.RestoreDeviceObjects(pd3dDevice);
	trueSkyRenderer.RestoreDeviceObjects(&renderPlatformDx11);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	trueSkyRenderer.InvalidateDeviceObjects();
	renderPlatformDx11.InvalidateDeviceObjects();
}

D3D_FEATURE_LEVEL Direct3D11Renderer::GetMinimumFeatureLevel() const
{
	return D3D_FEATURE_LEVEL_11_0;
}

int	Direct3D11Renderer::AddView(bool external_fb)
{
	return trueSkyRenderer.AddView(external_fb);
}

void Direct3D11Renderer::RemoveView(int view_id)
{
	return trueSkyRenderer.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	return trueSkyRenderer.ResizeView(view_id,pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
}
void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	simul::base::SetGpuProfilingInterface(pContext,&simul::dx11::Profiler::GetGlobalProfiler());
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=pContext;
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=view_id;
	trueSkyRenderer.Render(deviceContext);
}