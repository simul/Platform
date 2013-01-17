// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRendererDX1x.h A renderer for 2D cloud layers.

#pragma once
#include <d3dx9.h>
#ifdef DX10
	#include <D3D10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
#include "Simul/Base/SmartPtr.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"

//! A renderer for 2D cloud layers, e.g. cirrus clouds.
class Simul2DCloudRendererDX1x: public simul::clouds::Base2DCloudRenderer
{
public:
	Simul2DCloudRendererDX1x(simul::clouds::CloudKeyframer *ck2d);
	virtual ~Simul2DCloudRendererDX1x();
	void RestoreDeviceObjects(void*);
	void RecompileShaders();
	void InvalidateDeviceObjects();
	bool Render(bool cubemap,void *depth_tex,bool default_fog,bool write_alpha);
	void RenderCrossSections(int width,int height);
	void SetLossTexture(void *l);
	void SetInscatterTextures(void *i,void *s);
	void SetWindVelocity(float x,float y);
	//CloudShadowCallback
	void **GetCloudTextures(){return 0;}
	void *GetCloudShadowTexture() {return NULL;}
protected:
	void EnsureCorrectTextureSizes();
	void EnsureTexturesAreUpToDate();
	void EnsureTextureCycle();
	void EnsureCorrectIlluminationTextureSizes(){}
	void EnsureIlluminationTexturesAreUpToDate(){}
	virtual bool CreateNoiseTexture(bool override_file=false){return true;}
};