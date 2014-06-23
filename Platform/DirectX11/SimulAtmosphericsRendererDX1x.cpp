// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRendererDX1x.cpp A renderer for skies, clouds and weather effects.
#define NOMINMAX
#include "SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"
#include <tchar.h>
#ifndef SIMUL_WIN8_SDK
#include <dxerr.h>
#endif
#include <string>
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "SimulCloudRendererDX1x.h"
#include "PrecipitationRenderer.h"
#include "Simul2DCloudRendererDX1x.h"
#include "SimulSkyRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

SimulAtmosphericsRendererDX1x::SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
{
}

SimulAtmosphericsRendererDX1x::~SimulAtmosphericsRendererDX1x()
{
	Destroy();
}

void SimulAtmosphericsRendererDX1x::RecompileShaders()
{
	BaseAtmosphericsRenderer::RecompileShaders();
}

void SimulAtmosphericsRendererDX1x::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	HRESULT hr=S_OK;
	BaseAtmosphericsRenderer::RestoreDeviceObjects(r);
	RecompileShaders();
}

HRESULT SimulAtmosphericsRendererDX1x::Destroy()
{
	InvalidateDeviceObjects();
	return S_OK;
}
