// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseLightningRenderer.h"

class SimulGLLightningRenderer: public simul::clouds::BaseLightningRenderer
{
public:
	SimulGLLightningRenderer(simul::clouds::LightningRenderInterface *lri);
	~SimulGLLightningRenderer();
	bool RestoreDeviceObjects();
	bool Render();
	bool InvalidateDeviceObjects();
	//! This function does nothing as Y is never the vertical in this implementation
	virtual void SetYVertical(bool ){}
protected:
	bool InitEffects();
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
	struct PosTexVert_t
	{
		float3 position;	
		float2 texCoords;
	};
	PosTexVert_t *lightning_vertices;

	GLuint				lightning_program;	
	GLuint				lightning_vertex_shader;
	GLuint				lightning_fragment_shader;
	GLuint				lightning_texture;
	GLuint				lightningTexture_param;
	//D3DXMATRIX						world,view,proj;

	bool CreateLightningTexture();
};