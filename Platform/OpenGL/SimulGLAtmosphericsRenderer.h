// Copyright (c) 2011-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#ifndef _SIMULGLATMOSPHERICSRENDERER
#define _SIMULGLATMOSPHERICSRENDERER
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include <stdint.h> // for intptr_t

namespace simul
{
	namespace opengl
	{
		SIMUL_OPENGL_EXPORT_CLASS SimulGLAtmosphericsRenderer : public simul::sky::BaseAtmosphericsRenderer
		{
		public:
			SimulGLAtmosphericsRenderer(simul::base::MemoryInterface *m);
			virtual ~SimulGLAtmosphericsRenderer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void InvalidateDeviceObjects();
			// Assign the clouds framebuffer texture
			void SetCloudsTexture(void* t)
			{
				clouds_texture=(GLuint)(uintptr_t)t;
			}
			void SetInputTextures(void* image,void* depth)
			{
				input_texture=(GLuint)(uintptr_t)image;
				depth_texture=(GLuint)(uintptr_t)depth;
			}
			//! Render the Atmospherics.
			void RenderAsOverlay(simul::crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH);
		private:
			bool initialized;

			GLuint godrays_program;

			GLuint input_texture,depth_texture;
			GLuint clouds_texture;

			FramebufferGL *framebuffer;
		};
	}
}
#endif
