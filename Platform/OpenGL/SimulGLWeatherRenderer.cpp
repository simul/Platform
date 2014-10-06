
#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#endif

#include "FramebufferGL.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "Simul/Clouds/BaseLightningRenderer.h"
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
#include "Simul/Camera/Camera.h"
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
	simul::sky::SkyKeyframer *sk=environment->skyKeyframer;
	simul::clouds::CloudKeyframer *ck3d=environment->cloudKeyframer;
	baseSkyRenderer						=::new(memoryInterface) SimulGLSkyRenderer(sk,memoryInterface);
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
	fb->SetDimensions(w,h,Downscale,AtmosphericDownscale);
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
											,bool buffered
											,crossplatform::Texture *mainDepthTexture
											,crossplatform::Texture* lowResDepthTexture
											,const sky::float4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite)
{
	crossplatform::TwoResFramebuffer *fb=GetFramebuffer(deviceContext.viewStruct.view_id);
	if(buffered)
	{
		int w,h,s,a;
		fb->GetDimensions(w,h,s,a);
		if(mainDepthTexture)
		{
			int S=s;
			if(mainDepthTexture->width)
				S=mainDepthTexture->width/lowResDepthTexture->width;
			if(S<1)
				S=1;
			fb->SetDimensions(mainDepthTexture->width,mainDepthTexture->length,s,a);
		}
	}
	void *context=deviceContext.platform_context;
	RenderCloudsLate=false;
	if(baseSkyRenderer)
	{
		baseSkyRenderer->EnsureTexturesAreUpToDate(context);
		baseSkyRenderer->Render2DFades(deviceContext);
	}
	buffered=(buffered&&fb&&!is_cubemap);
	UpdateSkyAndCloudHookup();
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderAsOverlay(deviceContext, mainDepthTexture,exposure,depthViewportXYWH);
#if 1
	if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		base2DCloudRenderer->Render(deviceContext,exposure,false,crossplatform::FAR_PASS,mainDepthTexture,false,depthViewportXYWH,sky::float4(0.f,0.f,1.f,1.f));
	}
	if(buffered)
	{
		fb->GetLowResFarFramebuffer()->Activate(deviceContext);
		fb->GetLowResFarFramebuffer()->Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.f:1.f);
	}
	math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	if(baseSkyRenderer)
	{
		float cloud_occlusion=0;
		if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
			cloud_occlusion=baseCloudRenderer->GetSunOcclusion(deviceContext,cam_pos);
		baseSkyRenderer->CalcSunOcclusion(deviceContext,cloud_occlusion);
	}
	// Do this AFTER sky render, to catch any changes to texture definitions:
	UpdateSkyAndCloudHookup();
	if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		baseCloudRenderer->Render(deviceContext,buffered?1.f:exposure,is_cubemap,crossplatform::FAR_PASS,mainDepthTexture,true,depthViewportXYWH,sky::float4(0.f,0.f,1.f,1.f));
	if(buffered)
	{
		fb->GetLowResFarFramebuffer()->Deactivate(deviceContext);
	//	setTexture(Utilities::GetSingleton().simple_program,"image_texure",0,(GLuint)baseFramebuffer->GetColorTex());
		glUseProgram(Utilities::GetSingleton().simple_program);
		//setParameter(
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_SRC_ALPHA);
		//fb->GetLowResFarFramebuffer()->Render(context,true);
		if(doFinalCloudBufferToScreenComposite)
		{
		GL_ERROR_CHECK
			glPushAttrib(GL_ALL_ATTRIB_BITS);
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			Ortho();
  
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)fb->GetLowResFarFramebuffer()->GetColorTex());
			glDisable(GL_ALPHA_TEST);
			glDepthMask(GL_FALSE);
		GL_ERROR_CHECK
			::DrawQuad(0,0,1,1);
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glPopAttrib();
		GL_ERROR_CHECK
			glBindTexture(GL_TEXTURE_2D,0);
		}
		glUseProgram(0);
	}
#endif
}
