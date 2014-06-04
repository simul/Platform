// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseLightningRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"

SIMUL_OPENGL_EXPORT_CLASS SimulGLLightningRenderer: public simul::clouds::BaseLightningRenderer
{
public:
	SimulGLLightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk);
	~SimulGLLightningRenderer();
	void RestoreDeviceObjects();
	void Render(simul::crossplatform::DeviceContext &deviceContext,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const void *cloud_depth_tex);
	void InvalidateDeviceObjects();
	void RecompileShaders();
protected:
	struct float2
	{
		float x,y;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
		}
	};
	struct float3
	{
		float x,y,z;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
			z=f[2];
		}
	};

	GLuint				lightning_program;
	GLuint				glow_program;
	GLuint				lightning_texture;
	GLuint				lightningTexture_param;

	bool CreateLightningTexture();
	bool enable_geometry_shaders;
};