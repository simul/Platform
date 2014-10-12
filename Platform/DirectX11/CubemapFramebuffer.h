#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#endif
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#endif

#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		//! A DirectX 11 class for rendering to a cubemap.
		SIMUL_DIRECTX11_EXPORT_CLASS CubemapFramebuffer:public Framebuffer
		{
		public:
			CubemapFramebuffer();
			bool CreateBuffers();
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void Activate(crossplatform::DeviceContext &);
			void ActivateColour(crossplatform::DeviceContext &,const float viewportXYWH[4]);
			void Deactivate(crossplatform::DeviceContext &context);
			void DeactivateDepth(crossplatform::DeviceContext &context);
			void Clear(crossplatform::DeviceContext &context, float, float, float, float, float, int mask = 0);
			void ClearColour(crossplatform::DeviceContext &context, float, float, float, float);
			ID3D11Texture2D *GetCopy(crossplatform::DeviceContext &deviceContext);
		protected:
		};
	}
}