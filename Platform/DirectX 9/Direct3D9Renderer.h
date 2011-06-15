#pragma once

#ifndef D3DCALLBACKINTERFACE_H
#define D3DCALLBACKINTERFACE_H
// Direct3D includes
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
class D3DCallbackInterface
{
public:
	virtual bool	IsDeviceAcceptable(D3DCAPS9* pCaps,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,bool bWindowed)=0;
	virtual bool    ModifyDeviceSettings(struct DXUTDeviceSettings* pDeviceSettings)=0;
	virtual HRESULT OnCreateDevice(IDirect3DDevice9* pd3dDevice,const D3DSURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual HRESULT OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual void    OnFrameMove(double fTime, float fTimeStep)=0;
	virtual void    OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep)=0;
	virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing)=0;
	virtual void    OnLostDevice()=0;
	virtual void    OnDestroyDevice()=0;
	virtual const TCHAR *GetDebugText() const=0;
};
#endif

#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX 9/Export.h"
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
class SimulWeatherRenderer;
class SimulHDRRenderer;
class SimulTerrainRenderer;
class SIMUL_DIRECTX9_EXPORT Direct3D9Renderer
	:public D3DCallbackInterface
	,public simul::graph::meta::Group
{
public:
	Direct3D9Renderer();
	virtual ~Direct3D9Renderer();
	META_BeginProperties
		META_ValueProperty(float,Gamma,"")
		META_ValueProperty(float,Exposure,"")
	META_EndProperties
	SimulWeatherRenderer *GetSimulWeatherRenderer(){return simulWeatherRenderer.get();}
	SimulTerrainRenderer *GetSimulTerrainRenderer(){return simulTerrainRenderer.get();}
	void SetShowOSD(bool s);
	void SetCamera(simul::graph::camera::Camera *c);
//D3DCallbackInterface:
	bool	IsDeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat, bool bWindowed);
	bool    ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings);
	HRESULT OnCreateDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc);
	HRESULT OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc);
	void    OnFrameMove(double fTime, float fTimeStep);
	void    OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep);
	LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing);
	void    KeyboardProc(UINT nChar, bool bKeyDown, bool bAltDown);
	void    OnLostDevice();
	void    OnDestroyDevice();
	
	void	SetShowCloudCrossSections(bool val);
	void	SetShowLightVolume(bool val);
	void	SetShowFades(bool val);
	void	SetCelestialDisplay(bool val);
	void	SetShowTerrain(bool val);
	void	SetShowMap(bool val);
	
	void	SetTimeMultiplier(float fTimeMult);

	void	SetYVertical(bool y);
protected:
	simul::graph::camera::Camera *camera;
	float aspect;
	bool y_vertical;
	bool show_osd;
	float framerate;
	bool celestial_display;
	bool render_light_volume;
	bool show_cloud_sections;
	bool show_fades;
	bool show_terrain;
	bool show_map;
	simul::base::SmartPtr<SimulWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulTerrainRenderer> simulTerrainRenderer;
	simul::base::SmartPtr<SimulHDRRenderer> simulHDRRenderer;
	const TCHAR *GetDebugText() const;
	int width,height;
	float time_mult;
};

#pragma warning(pop)