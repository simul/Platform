#define NOMINMAX
// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "PrecipitationRenderer.h"
#include "CreateEffectDX1x.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/ProfilingInterface.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
#include "Simul/Sky/SkyInterface.h"
#pragma optimize("",off)
using namespace simul;
using namespace dx11;
