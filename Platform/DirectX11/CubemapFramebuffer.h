#pragma once
#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <D3dx11effect.h>
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Clouds/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/spherical_harmonics_constants.sl"
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		//! A DirectX 11 class for rendering to a cubemap.
		SIMUL_DIRECTX11_EXPORT_CLASS CubemapFramebuffer:public BaseFramebuffer
		{
		public:
			CubemapFramebuffer();
			virtual ~CubemapFramebuffer();
			void SetWidthAndHeight(int w,int h);
			void SetFormat(int i);
			void SetDepthFormat(int){}
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(void* );
			void RecompileShaders();
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			void SetCurrentFace(int i);
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void ActivateViewport(void *context, float viewportX, float viewportY, float viewportW, float viewportH );
			void Activate(void *context );
			void ActivateColour(void*,const float viewportXYWH[4]);
			void Deactivate(void *context);
			void DeactivateDepth(void *context);
			void Render(bool){}
			void Clear(void *context,float,float,float,float,float,int mask=0);
			void ClearColour(void* context, float, float, float, float );
			void CopyToMemory(void* /*context*/,void *,int,int){}
			virtual void* GetColorTex()
			{
				return (void*)m_pCubeEnvMapSRV;
			}
			virtual void* GetDepthTex()
			{
				return (void*)NULL;
			}
			virtual void* GetDepthTex(int i)
			{
				return (void*)m_pCubeEnvDepthMapSRV[i];
			}
			bool IsValid()
			{
				return (m_pCubeEnvMapSRV != NULL);
			}
			void				GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const;
			ID3D11Texture2D		*GetCopy(void *context);
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			void				CalcSphericalHarmonics(void *context);
			StructuredBuffer<vec4> &GetSphericalHarmonics()
			{
				return sphericalHarmonics;
			}
			void SetBands(int b)
			{
				if(b>MAX_SH_BANDS)
					b=MAX_SH_BANDS;
				if(bands!=b)
				{
					bands=b;
					sphericalHarmonics.release();
				}
			}
		protected:
			int bands;
			//! The size of the 2D buffer the sky is rendered to.
			int Width,Height;
			ID3D11Texture2D								*stagingTexture;	// Only initialized if CopyToMemory or GetCopy invoked.
			ID3D11Device*								pd3dDevice;
			ID3D11RenderTargetView*						m_pOldRenderTarget;
			ID3D11DepthStencilView*						m_pOldDepthSurface;
			D3D11_VIEWPORT								m_OldViewports[4];

			// One Environment map texture, one Shader Resource View on it, and six Render Target Views on it.
			ID3D11Texture2D*							m_pCubeEnvMap;
			ID3D11ShaderResourceView*					m_pCubeEnvMapSRV;
			ID3D11RenderTargetView*						m_pCubeEnvMapRTV[6];

			// Six Depth map textures, each with a DepthStencilView and SRV
			ID3D11Texture2D*							m_pCubeEnvDepthMap[6];
			ID3D11DepthStencilView*						m_pCubeEnvDepthMapDSV[6];
			ID3D11ShaderResourceView*					m_pCubeEnvDepthMapSRV[6];

			int											current_face;

			DXGI_FORMAT									format;

			ConstantBuffer<SphericalHarmonicsConstants> sphericalHarmonicsConstants;
			StructuredBuffer<SphericalHarmonicsSample>	sphericalSamples;
			StructuredBuffer<vec4>						sphericalHarmonics;
			ID3DX11Effect								*sphericalHarmonicsEffect;
		};
	}
}