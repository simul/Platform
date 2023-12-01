#pragma once
#include "Platform/CrossPlatform/Framebuffer.h"
#include "Platform/DirectX12/Export.h"
#include "Platform/DirectX12/Texture.h"
#include "SimulDirectXHeader.h"

namespace platform
{
	namespace dx12
	{
		//! A DirectX 12 framebuffer class.
		class SIMUL_DIRECTX12_EXPORT Framebuffer : public crossplatform::Framebuffer
		{
		public:
			Framebuffer(const char *name);
			virtual ~Framebuffer();
			virtual void SetAntialiasing(int a);
			virtual void Activate(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void ActivateDepth(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void Deactivate(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual void DeactivateDepth(crossplatform::GraphicsDeviceContext &deviceContext);

		private:
			DXGI_SAMPLE_DESC mCachedMSAAState;
		};
	}
}
