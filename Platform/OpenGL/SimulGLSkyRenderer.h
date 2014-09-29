// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/GpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include <cstdlib>

namespace simul
{
	namespace sky
	{
		class SiderealSkyInterface;
		class AtmosphericScatteringInterface;
		class Sky;
		class FadeTableInterface;
		class SkyKeyframer;
		class OvercastCallback;
	}
	namespace opengl
	{
		//! A sky rendering class for OpenGL.
		SIMUL_OPENGL_EXPORT_CLASS SimulGLSkyRenderer : public simul::sky::BaseSkyRenderer
		{
		public:
			SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m);
			virtual ~SimulGLSkyRenderer();
			//standard ogl object interface functions

			//! Create the API-specific objects to be used in rendering. This is usually called from the SimulGLWeatherRenderer that
			//! owns this object.
			void						RestoreDeviceObjects(simul::crossplatform::RenderPlatform *);
			simul::sky::BaseGpuSkyGenerator *GetBaseGpuSkyGenerator(){return &gpuSkyGenerator;}
		protected:
			simul::opengl::GpuSkyGenerator	gpuSkyGenerator;

			bool		Render2DFades(simul::crossplatform::DeviceContext &deviceContext);
			void		CreateSkyTextures();

			bool					CreateSkyEffect();
			bool					RenderSkyToBuffer();

			GLuint					fade_3d_to_2d_program;
			GLint					planetTexture_param;
			GLint					planetLightDir_param;
			GLint					planetColour_param;
		};
	}
}

