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
			void SetWidthAndHeight(int w,int h);
			void SetFormat(crossplatform::PixelFormat f);
			void SetDepthFormat(crossplatform::PixelFormat f);
			bool IsValid() const;
			void SetUseFastRAM(bool colour,bool depth)
			{
				useESRAM=colour;
				useESRAMforDepth=depth;
			}
			void SetAntialiasing(int a)
			{
				if(numAntialiasingSamples!=a)
				{
					numAntialiasingSamples=a;
					InvalidateDeviceObjects();
				}
			}
			void SetGenerateMips(bool);
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(crossplatform::RenderPlatform	*renderPlatform);
			bool CreateBuffers();
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			void MoveToFastRAM();
			void MoveToSlowRAM();
			void MoveDepthToSlowRAM();
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void Activate(crossplatform::DeviceContext &deviceContext);
			void ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4]);
			void ActivateDepth(crossplatform::DeviceContext &deviceContext);
			void ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH );
			void ActivateColour(crossplatform::DeviceContext &deviceContext);
			void Deactivate(crossplatform::DeviceContext &deviceContext);
			void DeactivateDepth(crossplatform::DeviceContext &deviceContext);
			void Clear(crossplatform::DeviceContext &context,float,float,float,float,float,int mask=0);
			void ClearDepth(crossplatform::DeviceContext &context,float);
			void ClearColour(crossplatform::DeviceContext &context, float, float, float, float );
			ID3D11ShaderResourceView *GetBufferResource()
			{
				return buffer_texture->AsD3D11ShaderResourceView();
			}
			void* GetColorTex()
			{
				return (void*)buffer_texture->AsD3D11ShaderResourceView();
			}
			//! Get the API-dependent pointer or id for the depth buffer target.
			ID3D11ShaderResourceView* GetDepthSRV()
			{
				return buffer_depth_texture->AsD3D11ShaderResourceView();
			}
			ID3D11Texture2D* GetColorTexture()
			{
				return (ID3D11Texture2D*)buffer_texture->AsD3D11Texture2D();
			}
			ID3D11Texture2D* GetDepthTexture2D()
			{
				return (ID3D11Texture2D*)buffer_depth_texture->AsD3D11Texture2D();
			}
			void GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const;
			dx11::Texture *GetTexture()
			{
				return (dx11::Texture*)buffer_texture;
			}
			Texture *GetDepthTexture()
			{
				return (dx11::Texture*)buffer_depth_texture;
			}
		protected:
			crossplatform::PixelFormat target_format;
			crossplatform::PixelFormat depth_format;
			bool Destroy();
		protected:
			
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
			unsigned							num_OldViewports;
			//! The texture the scene is rendered to.
		public:
			crossplatform::Texture				*buffer_texture;
		protected:
			bool useESRAM,useESRAMforDepth;
			//! The depth buffer.
			crossplatform::Texture				*buffer_depth_texture;
			bool IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
			float timing;
			bool GenerateMips;
			void SaveOldRTs(void *context);
			void SetViewport(void *context,float X,float Y,float W,float H,float Z,float D);
		};
	}
}
