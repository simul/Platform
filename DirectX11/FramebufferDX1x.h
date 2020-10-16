#pragma once
#include "DirectXHeader.h"
#include "Platform/DirectX11/MacrosDx1x.h"
#include "Platform/DirectX11/Export.h"
#include "Platform/DirectX11/Texture.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"

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
			virtual void Activate(crossplatform::GraphicsDeviceContext &deviceContext) override;
			virtual void ActivateDepth(crossplatform::GraphicsDeviceContext &deviceContext) override;
			virtual void Deactivate(crossplatform::GraphicsDeviceContext &deviceContext) override;
			virtual void DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext) override;
			virtual void Clear(crossplatform::GraphicsDeviceContext &context,float,float,float,float,float,int mask=0) override;
			virtual void ClearDepth(crossplatform::GraphicsDeviceContext &context,float) ;
			virtual void ClearColour(crossplatform::GraphicsDeviceContext &context, float, float, float, float ) override;

		protected:
			bool useESRAM,useESRAMforDepth;
		protected:
			void SaveOldRTs(crossplatform::GraphicsDeviceContext &deviceContext);
			void SetViewport(crossplatform::GraphicsDeviceContext &deviceContext,float X,float Y,float W,float H,float Z=0.0f,float D=1.0f);
		};
	}
}
