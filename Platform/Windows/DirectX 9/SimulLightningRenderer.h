// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseLightningRenderer.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
typedef long HRESULT;
class SimulLightningRenderer: public simul::clouds::BaseLightningRenderer
{
public:
public:
	OgreSimulLightningObject(simul::clouds::LightningRenderInterface *lri);
	~OgreSimulLightningObject();
	bool Update();
protected:
	simul::clouds::LightningRenderInterface *lightningRenderInterface;
};