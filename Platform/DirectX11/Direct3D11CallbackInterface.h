#pragma once
typedef enum D3D_FEATURE_LEVEL;
//! An interface to abstract the nuts and bolts of D3D device creation/management (e.g. what DXUT does) from the application-specific tasks of object allocation and rendering
//! (what the derived classes of this one will do).
class Direct3D11CallbackInterface
{
public:
	virtual bool				IsEnabled			() const=0;
	virtual D3D_FEATURE_LEVEL	GetMinimumFeatureLevel() const=0;
	virtual void				OnD3D11CreateDevice	(struct ID3D11Device* pd3dDevice)=0;
	//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called.
	virtual int					AddView				()=0;
	//! For a view that has already been created, this ensures that it has the requested size and format.
	virtual void				ResizeView			(int view_id,const struct DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
	//! Render the specified view. It's up to the renderer to decide what that means, and it's assumed that the render target and default depth buffer are already activated.
	virtual void				Render				(int view_id,struct ID3D11Device* pd3dDevice,struct ID3D11DeviceContext* pd3dImmediateContext)=0;
	virtual void				OnD3D11LostDevice	()=0;
	virtual void				OnD3D11DestroyDevice()=0;
	virtual void				RemoveView			(int)=0;
	virtual bool				OnDeviceRemoved		()=0;
	virtual void				OnFrameMove			(double fTime,float fTimeStep)=0;
	virtual const				char *GetDebugText	() const=0;
};

