#pragma once
#include "SimulDirectXHeader.h"
#include "Platform/DirectX12/Export.h"
#include "Platform/DirectX12/Texture.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"

namespace simul
{
	namespace dx12
	{
		//! A DirectX 12 framebuffer class.
		class SIMUL_DIRECTX12_EXPORT Framebuffer: public crossplatform::BaseFramebuffer
		{
		public:
			                Framebuffer(const char *name);
			virtual         ~Framebuffer();
			void            RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void            InvalidateDeviceObjects();
            virtual void    SetAntialiasing(int a);
			virtual void    Activate(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void    ActivateDepth(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void    Deactivate(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void    DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void    Clear(crossplatform::GraphicsDeviceContext &context,float,float,float,float,float,int mask=0);
			virtual void    ClearDepth(crossplatform::GraphicsDeviceContext &context,float);
			virtual void    ClearColour(crossplatform::GraphicsDeviceContext &context, float, float, float, float );
        private:
            DXGI_SAMPLE_DESC mCachedMSAAState;
		};
	}
}
