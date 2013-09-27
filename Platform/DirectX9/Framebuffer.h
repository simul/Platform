#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Clouds/BaseFramebuffer.h"

namespace simul
{
	namespace dx9
	{
		SIMUL_DIRECTX9_EXPORT_CLASS Framebuffer:public BaseFramebuffer
		{
		public:
			Framebuffer();
			~Framebuffer();
			void SetWidthAndHeight(int w,int h);
			void RestoreDeviceObjects(void *pd3dDevice);
			void InvalidateDeviceObjects();
			void Activate(void *);
			void ActivateViewport(void*,float viewportX, float viewportY, float viewportW, float viewportH);
			void Deactivate(void *);
			void DeactivateDepth(void*);
			void Clear(void *,float,float,float,float,float depth,int mask=0);
			void ClearColour(void *,float,float,float,float);
			void SetFormat(int f);
			void SetDepthFormat(int f);
			void* GetColorTex()
			{
				return (void*)buffer_texture;
			}
			void* GetDepthTex()
			{
				return (void*)buffer_depth_texture;
			}
			void CopyToMemory(void*,void*,int,int){}
		protected:
			LPDIRECT3DDEVICE9	m_pd3dDevice;
			LPDIRECT3DSURFACE9	m_pBufferDepthSurface;
			LPDIRECT3DSURFACE9	m_pOldRenderTarget;
			LPDIRECT3DSURFACE9	m_pOldDepthSurface;
			D3DFORMAT			texture_format;
			D3DFORMAT			depth_format;
			LPDIRECT3DTEXTURE9	buffer_texture;
			LPDIRECT3DTEXTURE9	buffer_depth_texture;
			LPDIRECT3DSURFACE9	m_pHDRRenderTarget;
			void MakeTexture();
		};
	}
}