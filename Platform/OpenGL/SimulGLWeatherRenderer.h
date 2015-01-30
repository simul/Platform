// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
typedef unsigned GLuint;
namespace simul
{
	namespace clouds
	{
		class Environment;
	}
}
#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif

namespace simul
{
	//! The namespace for the OpenGL platform library and its rendering classes.
	namespace opengl
	{
		class SimulGLSkyRenderer;
		class SimulGLCloudRenderer;
		//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within an OpenGL program.
		//! You can take this entire class and use it as source in your project.
		//! Make appropriate modifications where required.
		SIMUL_OPENGL_EXPORT_CLASS SimulGLWeatherRenderer:public simul::clouds::BaseWeatherRenderer
		{
		public:
			SimulGLWeatherRenderer(simul::clouds::Environment *env
								 ,simul::base::MemoryInterface *mem
								 ,int width=640,int height=480);
			virtual ~SimulGLWeatherRenderer();
			void SetScreenSize(int view_id,int w,int h);
			//! Call this when the device has been created
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void ReloadTextures();
			void RecompileShaders();
			//! Call this when the 3D device has been lost.
			void InvalidateDeviceObjects();
			//! Platform-dependent. Call this to draw the sky
			void RenderSkyAsOverlay(crossplatform::DeviceContext &deviceContext
											,bool is_cubemap
											,float exposure
											,float gamma
											,bool doLowResBufferRender
											,crossplatform::Texture *depthTexture
											,const vec4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite
											,vec2 pixelOffset) override;
			//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
			void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
			void EnableRain(bool e=true);
			void EnableCloudLayers();
		protected:
			std::string shader;
			//! This is set once the GL device has been initialized - then we can create textures and so forth.
			bool					device_initialized;
			typedef std::map<int,simul::crossplatform::TwoResFramebuffer*> FramebufferMap;
			FramebufferMap			framebuffers;
			bool					externally_defined_buffers;
			bool					auto_exposure;
			int						BufferWidth,BufferHeight;	//< The size of the 2D buffer the sky is rendered to.
			float					exposure;
			float					gamma;
			bool					use_buffer;
			bool					tone_map;
			RenderDepthBufferCallback *renderDepthBufferCallback;
			void CreateBuffers();
			void RenderBufferToScreen(GLuint texture,int w,int h,bool use_shader,bool blend=false);
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif