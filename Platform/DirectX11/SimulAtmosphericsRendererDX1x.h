// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
}

namespace simul
{
	namespace dx11
	{
		// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
		class SimulAtmosphericsRendererDX1x : public simul::sky::BaseAtmosphericsRenderer
		{
		public:
			SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m);
			virtual ~SimulAtmosphericsRendererDX1x();
			
			void SetBufferSize(int w,int h);

			// BaseAtmosphericsRenderer.
			void RecompileShaders();

			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r);
		protected:
			HRESULT Destroy();
		};
	}
}