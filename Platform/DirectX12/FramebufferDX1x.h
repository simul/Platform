#pragma once
#include "SimulDirectXHeader.h"
#include "Simul/Platform/DirectX12/MacrosDx1x.h"
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/DirectX12/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

namespace simul
{
	namespace dx12
	{
		//! A DirectX 12 framebuffer class.
		SIMUL_DIRECTX12_EXPORT_CLASS Framebuffer : public crossplatform::BaseFramebuffer
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
			virtual void ActivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void Deactivate(crossplatform::DeviceContext &deviceContext);
			virtual void DeactivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void Clear(crossplatform::DeviceContext &context,float,float,float,float,float,int mask=0);
			virtual void ClearDepth(crossplatform::DeviceContext &context,float);
			virtual void ClearColour(crossplatform::DeviceContext &context, float, float, float, float );

		protected:
			bool useESRAM,useESRAMforDepth;
		protected:
			void SaveOldRTs(crossplatform::DeviceContext &deviceContext);
			void SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z=0.0f,float D=1.0f);

			//! Holds the targets and viewports of this frame buffer, we push it into
			//! the frame buffer stack
			crossplatform::TargetsAndViewport							mTargetAndViewport;

			//! Viewport structure of this framebuffer
			D3D12_VIEWPORT												mViewport;

			//! We store the last pixel format so when we deactivate, we set it again
			DXGI_FORMAT													mLastPixelFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		};
	}
}
