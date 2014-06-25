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
			bool Create();
			void RecompileShaders();
			void RestoreDeviceObjects(crossplatform::RenderPlatform*);
			void InvalidateDeviceObjects();
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext);
			//! Render the clouds.
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,bool near_pass
				,crossplatform::Texture *depth_alpha_tex,bool write_alpha
				,const simul::sky::float4& viewportTextureRegionXYWH
				,const simul::sky::float4& mixedResTransformXYWH);
			//! Show the cross sections on-screen.
			void RenderCrossSections(crossplatform::DeviceContext &context,int x0,int y0,int width,int height);
			void RenderAuxiliaryTextures(crossplatform::DeviceContext &,int x0,int y0,int width,int height);
			void SetIlluminationTexture(crossplatform::Texture *i);
			simul::opengl::GpuCloudGenerator *GetGpuCloudGenerator(){return &gpuCloudGenerator;}
			simul::clouds::BaseGpuCloudGenerator *GetBaseGpuCloudGenerator(){return &gpuCloudGenerator;}
	
			CloudShadowStruct GetCloudShadowTexture(math::Vector3);
			const char *GetDebugText();
	
			void SetIlluminationGridSize(unsigned ,unsigned ,unsigned );
			void FillIlluminationSequentially(int ,int ,int ,const unsigned char *);
			void FillIlluminationBlock(int ,int ,int ,int ,int ,int ,int ,const unsigned char *);
			void GPUTransferDataToTexture(int index,unsigned char *target_texture
											,const unsigned char *direct_grid
											,const unsigned char *indirect_grid
											,const unsigned char *ambient_grid
											,bool wrap_light_tex);

			// a callback function that translates from daytime values to overcast settings. Used for
			// clouds to tell sky when it is overcast.
			simul::sky::OvercastCallback *GetOvercastCallback();
			//! Clear the sequence()
			void New();
		protected:
			simul::opengl::GpuCloudGenerator gpuCloudGenerator;
			void SwitchShaders(GLuint program);
			void DrawLines(void *,VertexXyzRgba *vertices,int vertex_count,bool strip);
			bool init;
			// Make up to date with respect to keyframer:
			void EnsureCorrectTextureSizes();
			void EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext);
			void EnsureCorrectIlluminationTextureSizes();
			void EnsureIlluminationTexturesAreUpToDate();
			void EnsureTextureCycle();

			GLuint clouds_background_program;
			GLuint clouds_foreground_program;
			GLuint raytrace_program;
			GLuint noise_prog;
			GLuint edge_noise_prog;
			GLuint current_program;
			void UseShader(GLuint program);

			GLuint cross_section_program;
			
#ifdef USE_GLFX
			GLint effect;
#endif
			GLuint cloud_shadow_program;
			GLint eyePosition_param;

			GLint cloudDensity1_param;
			GLint cloudDensity2_param;
			GLint noiseSampler_param;
			GLint lossSampler_param;
			GLint inscatterSampler_param;
			GLint illumSampler_param;
			GLint skylightSampler_param;
			GLint depthTexture;
	
			unsigned short *pIndices;

			simul::opengl::ConstantBuffer<CloudConstants> cloudConstants;
			simul::opengl::ConstantBuffer<CloudPerViewConstants> cloudPerViewConstants;
			simul::opengl::ConstantBuffer<LayerConstants> layerConstants;
			simul::opengl::ConstantBuffer<SingleLayerConstants> singleLayerConstants;

			GLint hazeEccentricity_param;
			GLint mieRayleighRatio_param;
	
			GLint		maxFadeDistanceMetres_param;

			simul::opengl::Texture	cloud_textures[3];
			// 2D texture
			GLuint		noise_tex;
			GLuint		volume_noise_tex;

			GLuint		sphere_vbo;
			GLuint		sphere_ibo;

			simul::opengl::Mesh sphereMesh;

			void CreateVolumeNoise();
			void CreateNoiseTexture(crossplatform::DeviceContext &deviceContext);
			bool CreateCloudEffect();
			bool RenderCloudsToBuffer();

			float texture_scale;
			float scale;
			float texture_effect;

			unsigned max_octave;
			bool BuildSphereVBO();
			opengl::Texture	cloud_shadow;
		};
	}
}
