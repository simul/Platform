#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#endif
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

namespace simul
{
	namespace dx11
	{
		//! A DirectX 11 framebuffer class.
		SIMUL_DIRECTX11_EXPORT_CLASS Framebuffer : public crossplatform::BaseFramebuffer
		{
		public:
			Framebuffer(const char *name);
			virtual ~Framebuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			virtual void SetUseFastRAM(bool colour,bool depth)
			{
				useESRAM=colour;
				useESRAMforDepth=depth;
			}
			virtual void SetAntialiasing(int a)
			{
				if(numAntialiasingSamples!=a)
				{
					numAntialiasingSamples=a;
					InvalidateDeviceObjects();
				}
			}
			//! Call this when the device has been lost.
			virtual void MoveToFastRAM();
			virtual void MoveToSlowRAM();
			virtual void MoveDepthToSlowRAM();
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			virtual void Activate(crossplatform::DeviceContext &deviceContext);
			virtual void ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4]);
			virtual void ActivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH );
			virtual void ActivateColour(crossplatform::DeviceContext &deviceContext);
			virtual void Deactivate(crossplatform::DeviceContext &deviceContext);
			virtual void DeactivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void Clear(crossplatform::DeviceContext &context,float,float,float,float,float,int mask=0);
			virtual void ClearDepth(crossplatform::DeviceContext &context,float);
			virtual void ClearColour(crossplatform::DeviceContext &context, float, float, float, float );

		protected:
			bool useESRAM,useESRAMforDepth;
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
			unsigned							num_OldViewports;
		protected:
			void SaveOldRTs(crossplatform::DeviceContext &deviceContext);
			void SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z=0.0f,float D=1.0f);
		};
	}
}
