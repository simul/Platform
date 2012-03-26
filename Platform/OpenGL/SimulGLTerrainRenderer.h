// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#ifndef _SIMULGLTERRAINRENDERER
#define _SIMULGLTERRAINRENDERER
#include "Simul/Base/Referenced.h"
#include "Simul/Platform/OpenGL/Export.h"
#include <GL/glew.h>

SIMUL_OPENGL_EXPORT_CLASS SimulGLTerrainRenderer : public simul::base::Referenced
{
public:
	SimulGLTerrainRenderer();
	virtual ~SimulGLTerrainRenderer();
	//standard ogl object interface functions
	void ReloadShaders();
	bool RestoreDeviceObjects();
	bool InvalidateDeviceObjects();
	// Interface
	void SetMaxFadeDistanceKm(float dist_km);
	//! Render the terrain.
	bool Render();
private:
	float max_fade_distance_metres;
	GLuint program;
	GLuint vertex_shader,fragment_shader;
	GLint eyePosition_param;
	GLint maxFadeDistanceMetres_param;
};

#endif