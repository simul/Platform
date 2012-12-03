#pragma once
// Direct3D includes
#ifdef DX10
	#include <D3D10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
#include <DXUT11\Core\dxut.h>
#include <dxutdevice11.h>
#include <dxerr.h>

#include "Simul/Platform/DirectX1x/Direct3D11CallbackInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX1x/Export.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace camera
	{
		class Camera;
	}
	namespace clouds
	{
		class Environment;
	}
}
class SimulWeatherRendererDX1x;
class SimulHDRRendererDX1x;
class SimulTerrainRendererDX1x;
class SimulOpticsRendererDX1x;

class SIMUL_DIRECTX1x_EXPORT Direct3D11Renderer
	:public Direct3D11CallbackInterface
	,public simul::graph::meta::Group
{
public:
	Direct3D11Renderer(simul::clouds::Environment *env,int w,int h);
	virtual ~Direct3D11Renderer();
	META_BeginProperties
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")
		META_ValueProperty(bool,ShowCloudCrossSections,"Show the cloud textures as an overlay.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
		META_ValueProperty(bool,UseHdrPostprocessor,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
		META_ValueProperty(bool,UseSkyBuffer,"Render the sky to a low-res buffer to increase performance.")
		META_ValueProperty(bool,ShowLightVolume,"Show the cloud light volume as a wireframe box.")
		META_ValueProperty(bool,CelestialDisplay,"Show geographical and sidereal overlay.")
		META_ValueProperty(bool,ShowWater,"Show water surfaces.")
	META_EndProperties
	bool IsEnabled()const{return enabled;}
	class SimulWeatherRendererDX1x *GetSimulWeatherRenderer()
	{
		return simulWeatherRenderer.get();
	}
	class SimulHDRRendererDX1x *GetSimulHDRRenderer()
	{
		return simulHDRRenderer.get();
	}

	void SetCamera(simul::camera::Camera *c)
	{
		camera=c;
	}
	void	SetYVertical(bool y);
	void	RecompileShaders();
	// D3D11CallbackInterface
	virtual bool	IsD3D11DeviceAcceptable(	const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed);
	virtual bool	ModifyDeviceSettings(		DXUTDeviceSettings* pDeviceSettings);
	virtual HRESULT	OnD3D11CreateDevice(		ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	virtual HRESULT	OnD3D11ResizedSwapChain(	ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	virtual void	OnD3D11FrameRender(		ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep);
	virtual void	OnD3D11LostDevice();
	virtual void	OnD3D11DestroyDevice();
	virtual void	OnD3D11ReleasingSwapChain();
	virtual bool	OnDeviceRemoved();
	virtual void    OnFrameMove(double fTime,float fTimeStep);
	virtual const	char *GetDebugText() const;
protected:
	bool enabled;
	bool y_vertical;
	simul::camera::Camera *camera;
	float aspect;
	simul::base::SmartPtr<SimulOpticsRendererDX1x>	simulOpticsRenderer;
	simul::base::SmartPtr<SimulWeatherRendererDX1x>	simulWeatherRenderer;
	simul::base::SmartPtr<SimulHDRRendererDX1x>		simulHDRRenderer;
	float timeMult;
	unsigned ScreenWidth,ScreenHeight;
};	

#pragma warning(pop)