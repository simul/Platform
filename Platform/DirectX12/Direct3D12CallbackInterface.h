#pragma once
//! Forward declaration for this, means we needn't include the dx11 headers yet.
enum D3D_FEATURE_LEVEL;
namespace simul
{
	namespace dx11on12
	{
		//! An interface to abstract the nuts and bolts of D3D device creation/management (e.g. what DXUT does) from the application-specific tasks of object allocation and rendering
		//! (what the derived classes of this one will do).
		class PlatformRendererInterface
		{
		public:
			virtual void				OnD3D11CreateDevice	(struct ID3D11Device* pd3dDevice)=0;
			//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
			virtual int					AddView				(bool external_fb)=0;
			virtual void				RemoveView			(int)=0;
			//! For a view that has already been created, this ensures that it has the requested size and format.
			virtual void				ResizeView			(int view_id,const struct DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)=0;
			//! Render the specified view. It's up to the renderer to decide what that means, and it's assumed that the render target and default depth buffer are already activated.
			//! If a depth buffer is passed, 
			virtual void				Render				(int view_id
																,struct ID3D11Device* pd3dDevice
																,struct ID3D11DeviceContext* pd3dImmediateContext)=0;
		};
	}
}