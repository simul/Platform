#pragma once
#include "SimulDirectXHeader.h"
#include "Simul/Platform/CrossPlatform/MixedResolutionView.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/mixed_resolution_constants.sl"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Camera/Camera.h"
#include <set>

namespace simul
{
	namespace dx11
	{
		enum ViewType
		{
			MAIN_3D_VIEW
			,OCULUS_VR
		};
		//! A class that encapsulates the generated mixed-resolution depth textures, and (optionally) a framebuffer with colour and depth.
		//! One instance of MixedResolutionView will be created and maintained for each live 3D view.
		struct SIMUL_DIRECTX11_EXPORT MixedResolutionView:public crossplatform::MixedResolutionView
		{
			MixedResolutionView();
			~MixedResolutionView();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			int GetScreenWidth() const;
			int GetScreenHeight() const;
			void SetResolution(int w,int h);
			void SetExternalFramebuffer(bool);
			// If an external depth buffer is used, pass it here, wrappered in a simul::crossplatform::Texture.
			void SetExternalDepthTexture(crossplatform::Texture *tex);
			void ResolveFramebuffer(crossplatform::DeviceContext &deviceContext);
			// Debuggin onscreen info:
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,int x0,int y0,int dx,int dy);
			ID3D11ShaderResourceView *MixedResolutionView::GetResolvedHDRBuffer();
			crossplatform::BaseFramebuffer			*GetFramebuffer()
			{
				return &hdrFramebuffer;
			}
			crossplatform::Texture					*GetHiResDepthTexture()
			{
				return &hiResDepthTexture;
			}
			crossplatform::Texture					*GetLowResDepthTexture()
			{
				return &lowResDepthTexture;
			}
			crossplatform::Texture					*GetLowResScratchTexture()
			{
				return &lowResScratch;
			}
			ViewType						viewType;
		//private:
			// A framebuffer with depth
			simul::dx11::Framebuffer		hdrFramebuffer;
			// The depth from the HDR framebuffer can be resolved into this texture:
			simul::dx11::Texture			hiResDepthTexture;
			simul::dx11::Texture			lowResDepthTexture;
			simul::dx11::Texture			lowResScratch;
			simul::dx11::Texture			resolvedTexture;
			crossplatform::RenderPlatform	*renderPlatform;
			const simul::camera::CameraOutputInterface	*camera;
		public:
			int								ScreenWidth;
			int								ScreenHeight;
			bool							useExternalFramebuffer;
			crossplatform::Texture			*externalDepthTexture;
		};
		class SIMUL_DIRECTX11_EXPORT MixedResolutionRenderer
		{
		public:
			MixedResolutionRenderer();
			~MixedResolutionRenderer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			void DownscaleDepth(crossplatform::DeviceContext &deviceContext,MixedResolutionView *view,int s,vec3 depthToLinFadeDistParams);
		protected:
			crossplatform::RenderPlatform				*renderPlatform;
			crossplatform::Effect						*depthForwardEffect;
			crossplatform::Effect						*depthReverseEffect;
			crossplatform::ConstantBuffer<MixedResolutionConstants>	mixedResolutionConstants;
		};
		/// A class to store a set of MixedResolutionView objects, one per view id.
		class SIMUL_DIRECTX11_EXPORT MixedResolutionViewManager
		{
		public:
			MixedResolutionViewManager():
				last_created_view_id(-1)
				,renderPlatform(NULL)
			{}
			void							RestoreDeviceObjects	(crossplatform::RenderPlatform *renderPlatform);
			void							InvalidateDeviceObjects	();
			void							RecompileShaders		();
			void							DownscaleDepth			(crossplatform::DeviceContext &deviceContext,int s,float max_dist_metres);
			MixedResolutionView				*GetView				(int view_id);
			std::set<MixedResolutionView*>	GetViews				();
			int								AddView					(bool external_framebuffer);
			void							RemoveView				(int);
			void							Clear					();
		protected:
			crossplatform::RenderPlatform				*renderPlatform;
			typedef std::map<int,MixedResolutionView*>	ViewMap;
			ViewMap							views;
			int								last_created_view_id;
			MixedResolutionRenderer			mixedResolutionRenderer;
		};
	}
}