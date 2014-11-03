// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include "SimulDirectXHeader.h"

#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Effect.h"

#pragma warning(push)
#pragma warning(disable:4251)

namespace simul
{
	namespace dx11
	{
		//! A class that provides gamma-correction of an HDR render to the screen.
		SIMUL_DIRECTX11_EXPORT_CLASS SimulHDRRendererDX1x: public simul::crossplatform::HdrRenderer
		{
		public:
			SimulHDRRendererDX1x();
		};
	}
}
#pragma warning(pop)