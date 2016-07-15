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
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
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
		:clouds::TrueSkyRenderer(env,sc,m,true)
{
	sc;
	ReverseDepthChanged();
}

TrueSkyRenderer::~TrueSkyRenderer()
{
	InvalidateDeviceObjects();
}

void TrueSkyRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	clouds::TrueSkyRenderer::RestoreDeviceObjects(r);
	if(!renderPlatform)
		return;
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

void Direct3D11Renderer::OnD3D11CreateDevice	(ID3D11Device* pd3dDevice)
{
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

void Direct3D11Renderer::RemoveView			(int view_id)
{
	return trueSkyRenderer.RemoveView(view_id);
}

void Direct3D11Renderer::ResizeView(int view_id,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	return trueSkyRenderer.ResizeView(view_id,pBackBufferSurfaceDesc->Width,pBackBufferSurfaceDesc->Height);
}

void Direct3D11Renderer::Render(int view_id,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext)
{
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=pContext;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatformDx11.GetGpuProfiler());
	deviceContext.renderPlatform	=&renderPlatformDx11;
	deviceContext.viewStruct.view_id=view_id;
	trueSkyRenderer.Render(deviceContext);
}