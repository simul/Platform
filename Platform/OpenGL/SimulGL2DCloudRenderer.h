// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Export.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class CloudKeyframer;
		class FastCloudNode;
	}
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
	namespace opengl
	{

		SIMUL_OPENGL_EXPORT_CLASS SimulGL2DCloudRenderer : public simul::clouds::Base2DCloudRenderer
		{
		public:
			SimulGL2DCloudRenderer(simul::clouds::CloudKeyframer *ck
													   ,simul::base::MemoryInterface *mem);
			virtual ~SimulGL2DCloudRenderer();
			//standard ogl object interface functions
			bool Create();
			//! OpenGL Implementation of device object creation - needs a GL context to be present.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void RecompileShaders();
			//! OpenGL Implementation of device invalidation - not strictly needed in GL.
			void InvalidateDeviceObjects();
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext);
			//! OpenGL Implementation of 2D cloud rendering.
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,bool near_pass,crossplatform::Texture *depth_alpha_tex,bool write_alpha,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& );
			void RenderCrossSections(crossplatform::DeviceContext &context,int x0,int y0,int width,int height);
			void RenderAuxiliaryTextures(crossplatform::DeviceContext &,int ,int ,int ,int ){}
			void SetWindVelocity(float x,float y);

			void SetCloudTextureSize(unsigned width_x,unsigned length_y);
			void FillCloudTextureBlock(int texture_index,int x,int y,int w,int l,const unsigned *uint32_array);
			void FillCloudTextureSequentially(int,int,int,const unsigned int *){}

		protected:
			virtual void DrawLines(void *,VertexXyzRgba *,int ,bool ){}

			void EnsureCorrectTextureSizes();
			void EnsureTexturesAreUpToDate(crossplatform::DeviceContext &);
			void EnsureTextureCycle();
			void EnsureCorrectIlluminationTextureSizes(){}
			void EnsureIlluminationTexturesAreUpToDate(){}
	
			GLuint clouds_program;
	
			GLuint cross_section_program;

			GLint globalScale;
			GLint detailScale;
			GLint offsetScale;
			GLint origin;

			GLint imageTexture_param;
			GLint lossTexture;
			GLint inscatterSampler_param;
			GLint skylightSampler_param;
	
			GLint cloud2DConstants;
			GLuint cloud2DConstantsUBO;
			GLint cloud2DConstantsBindingIndex;
	
			GLint earthShadowUniforms;

			GLint coverageTexture;
	
			GLuint	coverage_tex[3];
	
			FramebufferGL	coverage_fb;

			FramebufferGL	detail_fb;
			void CreateNoiseTexture(crossplatform::DeviceContext &deviceContext);
			//void CreateImageTexture();
			bool CreateCloudEffect();
			bool RenderCloudsToBuffer();

			float texture_scale;
			float scale;
			float texture_effect;
			unsigned char *cloud_data;
			unsigned tex_width;
		};
	}
}