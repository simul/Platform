
#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#endif

#include "FramebufferGL.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "Simul/Clouds/BaseLightningRenderer.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/TwoResFramebuffer.h"
#include "Simul/Math/Decay.h"
#include <stdint.h>  // for uintptr_t

using namespace simul;
using namespace opengl;

static const GLenum buffer_tex_format		=GL_FLOAT;

SimulGLWeatherRenderer::SimulGLWeatherRenderer(simul::clouds::Environment *env
											   ,simul::base::MemoryInterface *mem
											   ,int 
											   ,int )
		:BaseWeatherRenderer(env,mem)
		,BufferWidth(0)
		,BufferHeight(0)
		,device_initialized(false)
{
	simul::clouds::CloudKeyframer *ck3d=environment->cloudKeyframer;
	del(baseCloudRenderer,memoryInterface);
	baseCloudRenderer					=::new(memoryInterface) SimulGLCloudRenderer(ck3d,mem);

	EnableCloudLayers();
}


void SimulGLWeatherRenderer::EnableCloudLayers()
{
	if(base2DCloudRenderer)
	{
		if(baseSkyRenderer)
			base2DCloudRenderer->SetSkyInterface(baseSkyRenderer->GetBaseSkyInterface());
	}
	if(device_initialized)
	{
GL_ERROR_CHECK
		if(baseSkyRenderer)
			baseSkyRenderer->RestoreDeviceObjects(renderPlatform);
GL_ERROR_CHECK
		if(basePrecipitationRenderer)
			basePrecipitationRenderer->RestoreDeviceObjects(renderPlatform);
GL_ERROR_CHECK
		if(base2DCloudRenderer)
			base2DCloudRenderer->RestoreDeviceObjects(renderPlatform);
GL_ERROR_CHECK
		if(baseCloudRenderer)
			baseCloudRenderer->RestoreDeviceObjects(renderPlatform);
		if(baseLightningRenderer)
			baseLightningRenderer->RestoreDeviceObjects(renderPlatform);
	}
	ConnectInterfaces();
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLWeatherRenderer::SetScreenSize(int view_id,int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
	crossplatform::TwoResFramebuffer *fb=GetFramebuffer(view_id);
	fb->SetDimensions(w,h,Downscale);
	//scene_buffer->InitColor_Tex(0,internal_buffer_format);
	//fb->RestoreDeviceObjects(0);
}

void SimulGLWeatherRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseWeatherRenderer::RestoreDeviceObjects(r);
ERRNO_CHECK
	glewInit();
ERRNO_CHECK
	if(!CheckExtension("GL_VERSION_2_0"))
		throw simul::base::RuntimeError("OpenGL version 2.0 is not supported on this hardware");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");
ERRNO_CHECK
GL_ERROR_CHECK
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
	{
		i->second->RestoreDeviceObjects(renderPlatform);
	}
GL_ERROR_CHECK
	device_initialized=true;
	EnableCloudLayers();
ERRNO_CHECK
}

void SimulGLWeatherRenderer::InvalidateDeviceObjects()
{
	BaseWeatherRenderer::InvalidateDeviceObjects();
}

void SimulGLWeatherRenderer::ReloadTextures()
{
	BaseWeatherRenderer::ReloadTextures();
}

void SimulGLWeatherRenderer::RecompileShaders()
{
	BaseWeatherRenderer::RecompileShaders();
}

void SimulGLWeatherRenderer::RenderSkyAsOverlay(crossplatform::DeviceContext &deviceContext
											,bool is_cubemap
											,float exposure
											,float gamma
											,bool doLowResBufferRender
											,crossplatform::Texture *depthTexture
											,const vec4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite
											,vec2 pixelOffset)
{
}
