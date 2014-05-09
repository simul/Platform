#pragma once
#include <d3d11.h>
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Camera/Camera.h"

namespace simul
{
	namespace dx11
	{
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
			,FADE_EDITING
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of View will be created and maintained for each live 3D view.
		struct SIMUL_DIRECTX11_EXPORT View
		{
			View();
			~View();
			void RestoreDeviceObjects(ID3D11Device *pd3dDevice);
			void InvalidateDeviceObjects();
			int GetScreenWidth() const;
			int GetScreenHeight() const;
			void SetResolution(int w,int h);
			void SetExternalFramebuffer(bool);
			void SetExternalDepthResource(ID3D11ShaderResourceView *tex);
			void ResolveFramebuffer(ID3D11DeviceContext *pContext);
			ID3D11ShaderResourceView *View::GetResolvedHDRBuffer();
			// A framebuffer with depth
			simul::dx11::Framebuffer					hdrFramebuffer;
			// The depth from the HDR framebuffer can be resolved into this texture:
			simul::dx11::TextureStruct					hiResDepthTexture;
			simul::dx11::TextureStruct					lowResDepthTexture;
			ViewType									viewType;
			const simul::camera::CameraOutputInterface	*camera;
		private:
			simul::dx11::TextureStruct		resolvedTexture;
			ID3D11Device					*m_pd3dDevice;
		public:
			int								ScreenWidth;
			int								ScreenHeight;
			bool							useExternalFramebuffer;
			//ID3D11Texture2D					*externalDepthTexture;
			ID3D11ShaderResourceView		*externalDepthTexture_SRV;
		};
		class SIMUL_DIRECTX11_EXPORT ViewManager
		{
		public:
			ViewManager():
				last_created_view_id(-1)
			{}
			View						*GetView(int view_id);
			int							AddView				(bool external_framebuffer);
			void						RemoveView			(int);
			void						Clear();
		protected:
			typedef std::map<int,View*>	ViewMap;
			ViewMap						views;
			int							last_created_view_id;
		};
	}
}