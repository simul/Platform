#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
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
			Framebuffer(int w=0,int h=0);
			virtual ~Framebuffer();
			virtual void SetWidthAndHeight(int w,int h);
			virtual void SetAsCubemap(int w);
			virtual void SetCubeFace(int f);
			virtual void SetFormat(crossplatform::PixelFormat f);
			virtual void SetDepthFormat(crossplatform::PixelFormat f);
			bool IsValid() const;
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
			virtual void SetGenerateMips(bool);
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform	*renderPlatform);
			bool CreateBuffers();
			void RecompileShaders();
			//! Call this when the device has been lost.
			virtual void InvalidateDeviceObjects();
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
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			void CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext);
		protected:
			bool Destroy();
		protected:
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
			unsigned							num_OldViewports;

			// One Environment map texture, one Shader Resource View on it, and six Render Target Views on it.
			ID3D11RenderTargetView*				m_pCubeEnvMapRTV[6];
		protected:
			bool useESRAM,useESRAMforDepth;
			bool IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
			void SaveOldRTs(crossplatform::DeviceContext &deviceContext);
			void SetViewport(crossplatform::DeviceContext &deviceContext,float X,float Y,float W,float H,float Z=0.0f,float D=1.0f);
		};
	}
}
