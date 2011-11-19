#pragma once
#ifndef D3D1xCALLBACKINTERFACE_H
#define D3D1xCALLBACKINTERFACE_H
// Direct3D includes
#ifdef DX10
	#include <D3D10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Samples\C++\DXUT11\Core\dxut.h>
#include <dxutdevice11.h>
#include <dxerr.h>

#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX1x/Export.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace graph
	{
		namespace camera
		{
			class Camera;
		}
	}
}
class SimulWeatherRendererDX1x;
class SimulHDRRendererDX1x;
class SimulTerrainRendererDX1x;
class D3D11CallbackInterface
{
public:
	virtual bool	IsD3D11DeviceAcceptable(	const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed)=0;
	virtual bool	ModifyDeviceSettings(		DXUTDeviceSettings* pDeviceSettings)=0;
	virtual HRESULT	OnD3D11CreateDevice(		ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual HRESULT	OnD3D11ResizedSwapChain(	ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual void	OnD3D11FrameRender(		ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)=0;
	virtual void	OnD3D11LostDevice()=0;
	virtual void	OnD3D11DestroyDevice()=0;
	virtual void	OnD3D11ReleasingSwapChain()=0;
	virtual bool	OnDeviceRemoved()=0;
	virtual void    OnFrameMove(double fTime,float fTimeStep)=0;
	virtual const	char *GetDebugText() const=0;
};
#endif

class SIMUL_DIRECTX1x_EXPORT Direct3D11Renderer
	:public D3D11CallbackInterface
	,public simul::graph::meta::Group
{
public:
	Direct3D11Renderer(const char *license_key);
	virtual ~Direct3D11Renderer();
	META_BeginProperties
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
	META_EndProperties
	class SimulWeatherRendererDX1x *GetSimulWeatherRenderer()
	{
		return simulWeatherRenderer.get();
	}
	void SetCamera(simul::graph::camera::Camera *c)
	{
		camera=c;
	}
	void	SetYVertical(bool y);
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
	bool y_vertical;
	simul::graph::camera::Camera *camera;
	float aspect;
	simul::base::SmartPtr<SimulWeatherRendererDX1x> simulWeatherRenderer;
	simul::base::SmartPtr<SimulHDRRendererDX1x> simulHDRRenderer;
	float timeMult;
};

#pragma warning(pop)