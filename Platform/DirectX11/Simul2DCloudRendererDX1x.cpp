// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.
#define NOMINMAX
#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/Crossplatform/Layout.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/Crossplatform/Buffer.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

using namespace simul;
using namespace dx11;

Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::Base2DCloudRenderer(ck,mem)
{
}

Simul2DCloudRendererDX11::~Simul2DCloudRendererDX11()
{
	InvalidateDeviceObjects();
}
