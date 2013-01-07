// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX1x.cpp A renderer for 2D cloud layers.

#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX1x.h"


Simul2DCloudRendererDX1x::Simul2DCloudRendererDX1x(simul::clouds::CloudKeyframer *ck) :
	simul::clouds::Base2DCloudRenderer(ck)
{
}

Simul2DCloudRendererDX1x::~Simul2DCloudRendererDX1x()
{
InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX1x::RestoreDeviceObjects(void*)
{
}

void Simul2DCloudRendererDX1x::RecompileShaders()
{
}

void Simul2DCloudRendererDX1x::InvalidateDeviceObjects()
{
}

void Simul2DCloudRendererDX1x::EnsureCorrectTextureSizes()
{
}

void Simul2DCloudRendererDX1x::EnsureTexturesAreUpToDate()
{
}

void Simul2DCloudRendererDX1x::EnsureTextureCycle()
{
}

bool Simul2DCloudRendererDX1x::Render(bool cubemap,bool depth_testing,bool default_fog,bool write_alpha)
{
return true;
}

void Simul2DCloudRendererDX1x::RenderCrossSections(int width,int height)
{
}

void Simul2DCloudRendererDX1x::SetLossTexture(void *l)
{
}

void Simul2DCloudRendererDX1x::SetInscatterTextures(void *i,void *s)
{
}

void Simul2DCloudRendererDX1x::SetWindVelocity(float x,float y)
{
}
