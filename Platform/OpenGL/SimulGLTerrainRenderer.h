// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#ifndef _SIMULGLTERRAINRENDERER
#define _SIMULGLTERRAINRENDERER
#include "Simul/Base/Referenced.h"
#include "Simul/Terrain/BaseTerrainRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include <GL/glew.h>

SIMUL_OPENGL_EXPORT_CLASS SimulGLTerrainRenderer : public simul::terrain::BaseTerrainRenderer
{
public:
	SimulGLTerrainRenderer();
	virtual ~SimulGLTerrainRenderer();
	//standard ogl object interface functions
	void RecompileShaders();
	void RestoreDeviceObjects(void*);
	void InvalidateDeviceObjects();
	// Interface
	void SetMaxFadeDistanceKm(float dist_km);
	//! Render the terrain.
	void Render();
private:
	void MakeTextures();
	float max_fade_distance_metres;
	GLuint texArray;
	GLuint program;
	GLint eyePosition_param;
	GLint textures_param;
	GLint maxFadeDistanceMetres_param;
	GLint worldViewProj_param;
	GLint lightDir_param;
	GLint sunlight_param;
};

#endif