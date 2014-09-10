// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/hdr_constants.sl"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Graph/Meta/Group.h"

namespace simul
{
	namespace opengl
	{
		class Effect;
		SIMUL_OPENGL_EXPORT_CLASS SimulGLHDRRenderer
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
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			bool StartRender(crossplatform::DeviceContext &deviceContext);
			bool FinishRender(crossplatform::DeviceContext &deviceContext,float exposure,float gamma);
			void RenderGlowTexture(crossplatform::DeviceContext &deviceContext);
			FramebufferGL framebuffer;
		protected:
			crossplatform::RenderPlatform *renderPlatform;
			//__declspec(align(32))
			crossplatform::ConstantBuffer<HdrConstants> hdrConstants;
			FramebufferGL glow_fb;
			FramebufferGL alt_fb;
			bool initialized;
			// shaders
			crossplatform::Effect *effect;
			crossplatform::EffectTechnique *tech;
			GLuint tonemap_program;
			GLint exposure_param;
			GLint gamma_param;
			GLint buffer_tex_param;
			GLuint glow_program;
			GLuint blur_program;
		};
	}
}