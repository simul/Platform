// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/GpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/Mesh.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class CloudGeometryHelper;
		class FastCloudNode;
		class CloudKeyframer;
	}
	namespace sky
	{
		class BaseSkyInterface;
		class OvercastCallback;
	}
	namespace opengl
	{
		//! A renderer for clouds in OpenGL.
		SIMUL_OPENGL_EXPORT_CLASS SimulGLCloudRenderer : public simul::clouds::BaseCloudRenderer
		{
		public:
			SimulGLCloudRenderer(simul::clouds::CloudKeyframer *cloudKeyframer,simul::base::MemoryInterface *mem);
			virtual ~SimulGLCloudRenderer();
			//standard ogl object interface functions
			void RecompileShaders();
			void RestoreDeviceObjects(crossplatform::RenderPlatform*);
			void InvalidateDeviceObjects();
			//! Render the clouds.
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,crossplatform::NearFarPass nearFarPass
				,crossplatform::Texture *depth_alpha_tex,sky::ScatteringVolume *sv,bool write_alpha
				,const vec4& viewportTextureRegionXYWH
				,const vec4& mixedResTransformXYWH);
			//! Show the cross sections on-screen.
			void SetIlluminationTexture(crossplatform::Texture *i);
			simul::clouds::BaseGpuCloudGenerator *GetBaseGpuCloudGenerator(){return &gpuCloudGenerator;}
	
		protected:
			simul::opengl::GpuCloudGenerator gpuCloudGenerator;
			void SwitchShaders(GLuint program);
			bool init;
			// Make up to date with respect to keyframer:
			void EnsureCorrectTextureSizes();

			void UseShader(GLuint program);

			simul::opengl::Mesh sphereMesh;

			bool CreateCloudEffect();
			bool RenderCloudsToBuffer();

			float texture_scale;
			float scale;
			float texture_effect;

			unsigned max_octave;
			bool BuildSphereVBO();
		};
	}
}
