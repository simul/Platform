// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Graph/Meta/Group.h"

SIMUL_OPENGL_EXPORT_CLASS SimulGLHDRRenderer:public simul::graph::meta::Group
{
public:
	SimulGLHDRRenderer(int w,int h);
	~SimulGLHDRRenderer();
	void RecompileShaders();
	META_BeginProperties
		META_ValueProperty(float,Gamma,"")
		META_ValueProperty(float,Exposure,"")
	META_EndProperties
	void SetBufferSize(int w,int h);
	void RestoreDeviceObjects();
	void InvalidateDeviceObjects();
	bool StartRender();
	bool FinishRender();
protected:
	FramebufferGL *framebuffer;
};