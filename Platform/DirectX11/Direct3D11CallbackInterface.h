#pragma once

//! An interface to abstract the nuts and bolts of D3D device creation/management (e.g. what DXUT does) from the application-specific tasks of object allocation and rendering
//! (what the derived classes of this one will do).
class Direct3D11CallbackInterface
{
public:
	virtual bool				IsEnabled() const=0;
	virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const=0;
	virtual HRESULT				OnD3D11CreateDevice(	struct ID3D11Device* pd3dDevice,const struct DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual HRESULT				OnD3D11ResizedSwapChain(struct ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const struct DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual void				OnD3D11FrameRender(		struct ID3D11Device* pd3dDevice,struct ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)=0;
	virtual void				OnD3D11LostDevice()=0;
	virtual void				OnD3D11DestroyDevice()=0;
	virtual void				OnD3D11ReleasingSwapChain()=0;
	virtual bool				OnDeviceRemoved()=0;
	virtual void				OnFrameMove(double fTime,float fTimeStep)=0;
	virtual const				char *GetDebugText() const=0;
};

