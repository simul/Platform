#pragma once
#include "SimulDirectXHeader.h"
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
			virtual         ~Framebuffer();
			void            RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void            InvalidateDeviceObjects();
            virtual void    SetAntialiasing(int a);
			virtual void    Activate(crossplatform::DeviceContext &deviceContext);
			virtual void    ActivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void    Deactivate(crossplatform::DeviceContext &deviceContext);
			virtual void    DeactivateDepth(crossplatform::DeviceContext &deviceContext);
			virtual void    Clear(crossplatform::DeviceContext &context,float,float,float,float,float,int mask=0);
			virtual void    ClearDepth(crossplatform::DeviceContext &context,float);
			virtual void    ClearColour(crossplatform::DeviceContext &context, float, float, float, float );
		};
	}
}
