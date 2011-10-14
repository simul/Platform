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
	virtual const char *GetDebugText() const=0;
};
#endif

#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX9/Export.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace camera
	{
		class Camera;
	}
}
class SimulWeatherRenderer;
class SimulHDRRenderer;
class SimulTerrainRenderer;
class SimulOpticsRendererDX9;

class SIMUL_DIRECTX9_EXPORT Direct3D9Renderer
	:public D3DCallbackInterface
	,public simul::graph::meta::Group
{
public:
	Direct3D9Renderer(const char *license_key);
	virtual ~Direct3D9Renderer();
	META_BeginProperties
		META_ValueProperty(float,Gamma,"")
		META_ValueProperty(float,Exposure,"")
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")

		META_ValueProperty(bool,ShowCloudCrossSections,"Show cross-sections of the cloud volumes as an overlay.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
		META_ValueProperty(bool,ShowTerrain,"Whether to draw the terrain.")
		META_ValueProperty(bool,ShowMap,"Show the terrain map as an overlay.")

	META_EndProperties
	SimulWeatherRenderer *GetSimulWeatherRenderer(){return simulWeatherRenderer.get();}
	SimulTerrainRenderer *GetSimulTerrainRenderer(){return simulTerrainRenderer.get();}
	void SetShowOSD(bool s);
	void SetCamera(simul::camera::Camera *c);
	//! For picking
	void SetMousePosition(int x,int y,bool left,bool right,bool middle);
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
	
	void	SetShowLightVolume(bool val);
	void	SetCelestialDisplay(bool val);

	void	SetTimeMultiplier(float fTimeMult);

	void	SetYVertical(bool y);
	void	RecompileShaders();
protected:
	bool left_mouse,right_mouse,middle_mouse;
	HRESULT RestoreDeviceObjects(IDirect3DDevice9* pDevice);
	simul::camera::Camera *camera;
	D3DXVECTOR4 mouse_dir_viewspace;
	float aspect;
	bool device_reset;
	bool y_vertical;
	bool show_osd;
	float framerate;
	bool celestial_display;
	bool render_light_volume;
	simul::base::SmartPtr<SimulOpticsRendererDX9> simulOpticsRenderer;
	simul::base::SmartPtr<SimulWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulTerrainRenderer> simulTerrainRenderer;
	simul::base::SmartPtr<SimulHDRRenderer> simulHDRRenderer;
	const char *GetDebugText() const;
	int width,height;
	float time_mult;
};

#pragma warning(pop)