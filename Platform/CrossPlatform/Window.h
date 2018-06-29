#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace simul
{
	namespace crossplatform
	{
		struct SIMUL_CROSSPLATFORM_EXPORT Window
		{
			Window();
			~Window();
			void RestoreDeviceObjects(RenderPlatform* renderPlatform,bool m_vsync_enabled,int numerator,int denominator);
			void Release();
			void CreateRenderTarget();
			void CreateDepthBuffer();
			void SetRenderer(PlatformRendererInterface *ci, int view_id);
			void ResizeSwapChain(DeviceContext &deviceContext);
			cp_hwnd hwnd;
			/// The id assigned by the renderer to correspond to this hwnd
			int view_id;	
			bool vsync;
			SwapChain		*m_swapChain;
			Texture			*m_texture;
			RenderState		*m_rasterState;
			Viewport		viewport;
			PlatformRendererInterface *renderer;
		protected:
			RenderPlatform *renderPlatform;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif