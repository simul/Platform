// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef _MSC_VER
#include <GL/glew.h>
#endif
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

class SimulGLSkyRenderer;
class SimulGLLightningRenderer;
#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif
//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within a DirectX program.
//! You can take this entire class and use it as source in your project.
//! Make appropriate modifications where required.
SIMUL_OPENGL_EXPORT_CLASS SimulGLWeatherRenderer:public simul::clouds::BaseWeatherRenderer
{
public:
	SimulGLWeatherRenderer(const char *license_key,bool usebuffer=true,bool tonemap=false,int width=640,
		int height=480,bool sky=true,bool clouds3d=true,bool clouds2d=true,
		bool rain=true,
		bool colour_sky=false);
	virtual ~SimulGLWeatherRenderer();
	//! Call this when the device has been created
	bool RestoreDeviceObjects();
	//! Call this when the 3D device has been lost.
	bool InvalidateDeviceObjects();
	//! Platform-dependent. Call this to draw the sky
	bool RenderSky(bool buffered,bool is_cubemap);
	//! Call this to draw the clouds
	void RenderClouds(bool buffered,bool depth_testing,bool default_fog=false);
	//! Call this to draw lightning.
	void RenderLightning();
	//! Call this to draw rain etc.
	void RenderPrecipitation();
	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulGLSkyRenderer *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulGLCloudRenderer *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class SimulGL2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	//class SimulGLPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	//class SimulGLAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
	void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
	void EnableRain(bool e=true);
	void EnableLayers(bool,bool);
	void SetPrecipitation(float strength,float speed);
	//! Save a sky sequence (usually as a .seq file; this is how Sky Sequencer saves sequences).
	std::ostream &Save(std::ostream &os) const;
	//! Load a sequence (usually as a .seq file from Sky Sequencer).
	std::istream &Load(std::istream &is);
	//! Clear the sky sequence()
	void New();
	const char *GetDebugText() const;
protected:
	//! This is set once the GL device has been initialized - then we can create textures and so forth.
	bool device_initialized;
	class FramebufferGL *scene_buffer;
	bool AlwaysRenderCloudsLate;
	bool RenderCloudsLate;
	bool externally_defined_buffers;
	bool auto_exposure;
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	void CreateBuffers();
	void RenderBufferToScreen(GLuint texture,int w,int h,bool use_shader,bool blend=false);
	RenderDepthBufferCallback *renderDepthBufferCallback;
	simul::base::SmartPtr<class SimulGLSkyRenderer> simulSkyRenderer;
	simul::base::SmartPtr<class SimulGLCloudRenderer> simulCloudRenderer;
	simul::base::SmartPtr<class SimulGL2DCloudRenderer> simul2DCloudRenderer;
	simul::base::SmartPtr<class SimulGLLightningRenderer> simulLightningRenderer;
	//simul::base::SmartPtr<class SimulGLPrecipitationRenderer> simulPrecipitationRenderer;
	//simul::base::SmartPtr<class SimulGLAtmosphericsRenderer> simulAtmosphericsRenderer;
	float							exposure;
	float							gamma;
	bool							use_buffer;
	bool							tone_map;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif