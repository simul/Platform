// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRendererDX1x.h A renderer for skies.

#pragma once
#include "Simul/Sky/SkyTexturesCallback.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Math/Matrix4x4.h"
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"

struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
		class Sky;
		class SkyKeyframer;
	}
}

typedef long HRESULT;

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS SimulSkyRendererDX1x:public simul::sky::BaseSkyRenderer
		{
		public:
			SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem);
			//standard d3d object interface functions
			void RecompileShaders();
			//! Call this when the D3D device has been created or reset.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			//! Call this when the D3D device has been shut down.
			void InvalidateDeviceObjects();
			//! \deprecated This function is no longer used, as the sky is drawn by the atmospherics renderer. See simul::sky::BaseAtmosphericsRenderer.
			bool Render(void *context,bool blend);
			//! Draw the fade textures to screen
			bool RenderFades(crossplatform::DeviceContext &deviceContext,int x,int y,int w,int h);
			//! Call this to draw the sun flare, usually drawn last, on the main render target.
			bool RenderFlare(float exposure);
			//! Get a value, from zero to one, which represents how much of the sun is visible.
			//! Call this when the current rendering surface is the one that has obscuring
			//! objects like mountains etc. in it, and make sure these have already been drawn.
			//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
			//! uses a hardware occlusion query to see how many pixels have passed the z-test.

		//! Initialize textures
			void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);

			// for testing:
			void DrawCubemap(crossplatform::DeviceContext &deviceContext,ID3D11ShaderResourceView*	m_pCubeEnvMapSRV);
			simul::sky::BaseGpuSkyGenerator *GetBaseGpuSkyGenerator(){return &gpuSkyGenerator;}
		protected:
			void EnsureTexturesAreUpToDate(void *c);
			// A framebuffer where x=azimuth, y=elevation, r=start depth, g=end depth.
			simul::dx11::GpuSkyGenerator		gpuSkyGenerator;
		};
	}
}