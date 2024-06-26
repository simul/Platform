#pragma once
#include "DirectXHeader.h"
#include "Platform/DirectX11/MacrosDx1x.h"
#include "Platform/DirectX11/Export.h"
#include "Platform/DirectX11/Texture.h"
#include "Platform/CrossPlatform/Framebuffer.h"

namespace platform
{
	namespace dx11
	{
		//! A DirectX 11 framebuffer class.
		class SIMUL_DIRECTX11_EXPORT Framebuffer: public crossplatform::Framebuffer
		{
		public:
			Framebuffer(const char *name);
			virtual ~Framebuffer();
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

		protected:
			bool useESRAM,useESRAMforDepth;
		protected:
			void SaveOldRTs(crossplatform::GraphicsDeviceContext &deviceContext);
			void SetViewport(crossplatform::GraphicsDeviceContext &deviceContext,float X,float Y,float W,float H,float Z=0.0f,float D=1.0f);
		};
	}
}
