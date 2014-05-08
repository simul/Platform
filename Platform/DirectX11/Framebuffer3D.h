#pragma once

#include <d3d11.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "D3dx11effect.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS Framebuffer3D:public BaseFramebuffer
		{
		public:
			Framebuffer3D();
			~Framebuffer3D();
			// BaseFramebuffer
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void Activate(void*);
			void Deactivate(void*);
			void SetFormat(int);
			void Clear(void* context,float,float,float,float,float,int mask);
			void SetWidthAndHeight(int w,int h);
			void* GetColorTex();
			//
			void SetDepth(int d);
		protected:
			void CreateBuffers();
			int Depth;
			unsigned int num_v;
			DXGI_FORMAT target_format;
			ID3D11Device*						m_pd3dDevice;
			ID3D1xRenderTargetView*				renderTarget;
			ID3D11RenderTargetView*				m_pOldRenderTarget;
			ID3D11DepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
			ID3D11Texture3D*					texture;
			ID3D11ShaderResourceView*			texture_SRV;
		};
	}
}