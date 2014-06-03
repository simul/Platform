#pragma once
#include <d3d11.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "D3dx11effect.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
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
			void SetFormat(int f);
			void SetDepthFormat(int f);
			bool IsValid() const;
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
			void RestoreDeviceObjects(void* pd3dDevice);
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void Activate(void *context );
			void ActivateColour(void*,const float viewportXYWH[4]);
			void ActivateDepth(void *context);
			void ActivateViewport(void *context, float viewportX, float viewportY, float viewportW, float viewportH );
			void ActivateColour(void *context);
			void Deactivate(void *context);
			void DeactivateDepth(void *context);
			void Clear(void *context,float,float,float,float,float,int mask=0);
			void ClearDepth(void *context,float);
			void ClearColour(void* context, float, float, float, float );
			ID3D11ShaderResourceView *GetBufferResource()
			{
				return buffer_texture_SRV;
			}
			void* GetColorTex()
			{
				return (void*)buffer_texture_SRV;
			}
			void* GetDepthTex()
			{
				return (void*)buffer_depth_texture_SRV;
			}
			ID3D11Texture2D* GetColorTexture()
			{
				return hdr_buffer_texture;
			}
			ID3D11Texture2D* GetDepthTexture()
			{
				return buffer_depth_texture;
			}
			//! Copy from the rt to the given target memory. If not starting at the top of the texture (start_texel>0), the first byte written
			//! is at \em target, which is the address to copy the given chunk to, not the base address of the whole in-memory texture.
			void CopyToMemory(void *context,void *target,int start_texel=0,int texels=0);
			void GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const;
		protected:
			DXGI_FORMAT target_format;
			DXGI_FORMAT depth_format;
			bool Destroy();
			ID3D11Device*						m_pd3dDevice;

		public:
			ID3D1xRenderTargetView*				m_pHDRRenderTarget;
			ID3D1xDepthStencilView*				m_pBufferDepthSurface;
		protected:
			ID3D11Texture2D *stagingTexture;	// Only initialized if CopyToMemory is invoked.
			
			ID3D1xRenderTargetView*				m_pOldRenderTarget;
			ID3D1xDepthStencilView*				m_pOldDepthSurface;
			D3D1x_VIEWPORT						m_OldViewports[16];

			//! The texture the scene is rendered to.
		public:
			ID3D1xTexture2D*					hdr_buffer_texture;
			ID3D11ShaderResourceView*			buffer_texture_SRV;
		protected:
			//! The depth buffer.
			ID3D1xTexture2D*					buffer_depth_texture;
			ID3D11ShaderResourceView*			buffer_depth_texture_SRV;

			bool IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
			bool CreateBuffers();
			ID3D1xRenderTargetView* MakeRenderTarget(const ID3D1xTexture2D* pTexture);
			float timing;
			unsigned int num_v;
			bool GenerateMips;
			void SaveOldRTs(void *context);
			void SetViewport(void *context,float X,float Y,float W,float H,float Z,float D);
		};
	}
}
