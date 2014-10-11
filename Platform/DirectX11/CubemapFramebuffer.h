#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#endif
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#endif

#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/SL/spherical_harmonics_constants.sl"
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		//! A DirectX 11 class for rendering to a cubemap.
		SIMUL_DIRECTX11_EXPORT_CLASS CubemapFramebuffer:public crossplatform::CubemapFramebuffer
		{
		public:
			CubemapFramebuffer();
			virtual ~CubemapFramebuffer();
			void SetWidthAndHeight(int w,int h);
			void SetAntialiasing(int ){}
			void SetFormat(crossplatform::PixelFormat f);
			void SetDepthFormat(crossplatform::PixelFormat f);
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(crossplatform::RenderPlatform	*renderPlatform);
			bool CreateBuffers();
			void RecompileShaders();
			bool IsValid() const;
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			void SetCurrentFace(int i);
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void ActivateViewport(crossplatform::DeviceContext &, float viewportX, float viewportY, float viewportW, float viewportH );
			void Activate(crossplatform::DeviceContext &);
			void ActivateColour(crossplatform::DeviceContext &,const float viewportXYWH[4]);
			void ActivateDepth(crossplatform::DeviceContext &);
			void Deactivate(crossplatform::DeviceContext &context);
			void DeactivateDepth(crossplatform::DeviceContext &context);
			void Clear(crossplatform::DeviceContext &context, float, float, float, float, float, int mask = 0);
			void ClearColour(crossplatform::DeviceContext &context, float, float, float, float);
			bool IsValid()
			{
				return (texture->AsD3D11ShaderResourceView() != NULL);
			}
			ID3D11Texture2D		*GetCopy(crossplatform::DeviceContext &deviceContext);
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			void				CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext);
			crossplatform::StructuredBuffer<vec4> &GetSphericalHarmonics()
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
					sphericalHarmonics.InvalidateDeviceObjects();
				}
			}
			crossplatform::Texture *GetTexture()
			{
				return texture;
			}
			crossplatform::Texture *GetDepthTexture()
			{
				return depth_texture;//m_pCubeEnvDepthMap[current_face];
			}
		protected:
			int bands;
			//! The size of the 2D buffer the sky is rendered to.
			int Width,Height;
			ID3D11RenderTargetView*						m_pOldRenderTarget;
			ID3D11DepthStencilView*						m_pOldDepthSurface;
			D3D11_VIEWPORT								m_OldViewports[16];

			// One Environment map texture, one Shader Resource View on it, and six Render Target Views on it.
			crossplatform::Texture						*texture;
			ID3D11RenderTargetView*						m_pCubeEnvMapRTV[6];

			// Six Depth map textures, each with a DepthStencilView and SRV
			crossplatform::Texture*						depth_texture;//m_pCubeEnvDepthMap[6];

			int											current_face;

			crossplatform::PixelFormat					format,depth_format;

			crossplatform::ConstantBuffer<SphericalHarmonicsConstants> sphericalHarmonicsConstants;
			crossplatform::StructuredBuffer<SphericalHarmonicsSample>	sphericalSamples;
			crossplatform::StructuredBuffer<vec4>						sphericalHarmonics;
			crossplatform::Effect						*sphericalHarmonicsEffect;
		};
	}
}