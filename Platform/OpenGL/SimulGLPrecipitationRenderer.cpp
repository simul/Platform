// Copyright (c) 2011-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulGLPrecipitationRenderer.h"

#include "Simul/Base/SmartPtr.h"
#include "Simul/Math/Pi.h"
#include "Simul/Sky/Float4.h"

SimulGLPrecipitationRenderer::SimulGLPrecipitationRenderer() :
	external_rain_texture(false)
{
}

void SimulGLPrecipitationRenderer::TextureRepeatChanged()
{
}

bool SimulGLPrecipitationRenderer::RestoreDeviceObjects(void *dev)
{
	return  true;
}

bool SimulGLPrecipitationRenderer::SetExternalRainTexture(void* tex)
{
	return true;
}

bool SimulGLPrecipitationRenderer::InvalidateDeviceObjects()
{
	return true;
}

SimulGLPrecipitationRenderer::~SimulGLPrecipitationRenderer()
{
	InvalidateDeviceObjects();
}

bool SimulGLPrecipitationRenderer::Render()
{
	return  true;
}