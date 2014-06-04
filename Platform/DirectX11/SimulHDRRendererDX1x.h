// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include <d3d11.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "D3dx11effect.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/hdr_constants.sl"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Base/Referenced.h"
#include "Simul/Base/PropertyMacros.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		//! A class that provides gamma-correction of an HDR render to the screen.
		SIMUL_DIRECTX11_EXPORT_CLASS SimulHDRRendererDX1x: public simul::base::Referenced
		{
		public:
			SimulHDRRendererDX1x(int w,int h);
			virtual ~SimulHDRRendererDX1x();
			META_BeginProperties
				META_ValueProperty(bool,Glow,"Whether to apply a glow effect")
				META_ValuePropertyWithSetCall(bool,ReverseDepth,RecompileShaders,"")
			META_EndProperties
			void SetBufferSize(int w,int h);
			// Standard d3d object interface functions
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(void *x);
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			//! Render: write the given texture to screen using the HDR rendering shaders
			void Render(crossplatform::DeviceContext &deviceContext,void *texture_srv,float Exposure,float Gamma,float offsetX);
			void Render(crossplatform::DeviceContext &deviceContext,void *texture_srv,float Exposure,float Gamma);
			void RenderWithOculusCorrection(crossplatform::DeviceContext &deviceContext,void *texture_srv,float Exposure,float Gamma,float offsetX);
			//! Create the glow texture that will be overlaid due to strong lights.
			void RenderGlowTexture(crossplatform::DeviceContext &deviceContext,void *texture_srv);
			//! Get the current debug text as a c-string pointer.
			const char *GetDebugText() const;
			//! Get a timing value for debugging.
			float GetTiming() const;

			void RecompileShaders();
		protected:
			bool Destroy();
			simul::dx11::Framebuffer glow_fb;
			int Width,Height;
			ID3D11Device*						m_pd3dDevice;

			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			ID3DX11Effect*						m_pTonemapEffect;
			ID3DX11EffectTechnique*				exposureGammaTechnique;
			ID3DX11EffectTechnique*				glowExposureGammaTechnique;
			ID3DX11EffectTechnique*				warpExposureGamma;
			ID3DX11EffectTechnique*				warpGlowExposureGamma;
			
			ID3DX11EffectTechnique*				glowTechnique;
			ID3D1xEffectScalarVariable*			Exposure_;
			ID3D1xEffectScalarVariable*			Gamma_;

			ID3DX11Effect*						m_pGaussianEffect;
			ID3DX11EffectTechnique*				gaussianRowTechnique;
			ID3DX11EffectTechnique*				gaussianColTechnique;

			float timing;
			simul::dx11::ComputableTexture		glowTexture;
			ConstantBuffer<HdrConstants>		hdrConstants;
		};
	}
}
#pragma warning(pop)