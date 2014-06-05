
#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#endif

#include "FramebufferGL.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "SimulGLLightningRenderer.h"
#include "SimulGLAtmosphericsRenderer.h"
#include "SimulGLPrecipitationRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/Timer.h"
#include "Simul/Math/Decay.h"
#include <stdint.h>  // for uintptr_t

using namespace simul;
using namespace opengl;

static const GLenum buffer_tex_format		=GL_FLOAT;

TwoResFramebuffer::TwoResFramebuffer()
	:Width(0)
	,Height(0)
	,Downscale(0)
{
}

void TwoResFramebuffer::RestoreDeviceObjects(void *)
{
	if(Width==0||Height==0||Downscale==0)
		return;
	lowResFarFramebuffer	.SetFormat(GL_RGBA32F_ARB);
	lowResNearFramebuffer	.SetFormat(GL_RGBA32F_ARB);
	hiResFarFramebuffer		.SetFormat(GL_RGBA32F_ARB);
	hiResNearFramebuffer	.SetFormat(GL_RGBA32F_ARB);
	lowResFarFramebuffer	.SetDepthFormat(GL_DEPTH_COMPONENT32F);
	lowResNearFramebuffer	.SetDepthFormat(0);
	hiResFarFramebuffer		.SetDepthFormat(0);
	hiResNearFramebuffer	.SetDepthFormat(0);

	// Make sure the buffer is at least big enough to have Downscale main buffer pixels per pixel
	int BufferWidth		=(Width+Downscale-1)/Downscale;
	int BufferHeight	=(Height+Downscale-1)/Downscale;
	lowResFarFramebuffer	.SetWidthAndHeight(BufferWidth,BufferHeight);
	lowResNearFramebuffer	.SetWidthAndHeight(BufferWidth,BufferHeight);
	hiResFarFramebuffer		.SetWidthAndHeight(Width,Height);
	hiResNearFramebuffer	.SetWidthAndHeight(Width,Height);
	
/*	lowResFarFramebuffer	.RestoreDeviceObjects(NULL);
	lowResNearFramebuffer	.RestoreDeviceObjects(NULL);
	hiResFarFramebuffer		.RestoreDeviceObjects(NULL);
	hiResNearFramebuffer	.RestoreDeviceObjects(NULL);*/
}

void TwoResFramebuffer::InvalidateDeviceObjects()
{
	lowResFarFramebuffer	.InvalidateDeviceObjects();
	lowResNearFramebuffer	.InvalidateDeviceObjects();
	hiResFarFramebuffer		.InvalidateDeviceObjects();
	hiResNearFramebuffer	.InvalidateDeviceObjects();
}

void TwoResFramebuffer::SetDimensions(int w,int h,int downscale)
{
	if(Width!=w||Height!=h||Downscale!=downscale)
	{
		Width=w;
		Height=h;
		Downscale=downscale;
		RestoreDeviceObjects(NULL);
	}
}
void TwoResFramebuffer::GetDimensions(int &w,int &h,int &downscale)
{
	w=Width;
	h=Height;
	downscale=Downscale;
}


SimulGLWeatherRenderer::SimulGLWeatherRenderer(simul::clouds::Environment *env
											   ,simul::base::MemoryInterface *mem
											   ,int 
											   ,int )
		:BaseWeatherRenderer(env,mem)
		,BufferWidth(0)
		,BufferHeight(0)
		,device_initialized(false)
		,cloud_overlay_program(0)
{
	simul::sky::SkyKeyframer *sk=environment->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=environment->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=environment->cloudKeyframer;
	simulSkyRenderer					=::new(memoryInterface) SimulGLSkyRenderer(sk,memoryInterface);
	baseSkyRenderer=simulSkyRenderer;
	
	simulCloudRenderer					=::new(memoryInterface) SimulGLCloudRenderer(ck3d,mem);
	baseCloudRenderer=simulCloudRenderer;
	if(env->cloud2DKeyframer)
	{
		base2DCloudRenderer				=simul2DCloudRenderer
										=::new SimulGL2DCloudRenderer(ck2d,mem);
	}	
	simulLightningRenderer				=::new(memoryInterface) SimulGLLightningRenderer(ck3d,sk);
	baseLightningRenderer=simulLightningRenderer;

	simulAtmosphericsRenderer			=::new(memoryInterface) SimulGLAtmosphericsRenderer(mem);
	baseAtmosphericsRenderer			=simulAtmosphericsRenderer;
	basePrecipitationRenderer			=simulPrecipitationRenderer
										=::new(memoryInterface) SimulGLPrecipitationRenderer();

	EnableCloudLayers();
}


void SimulGLWeatherRenderer::EnableCloudLayers()
{
	if(simulCloudRenderer)
		simulCloudRenderer->Create();
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->Create();
		if(simulSkyRenderer)
			simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetBaseSkyInterface());
	}
	if(device_initialized)
	{
GL_ERROR_CHECK
		if(simulSkyRenderer)
			simulSkyRenderer->RestoreDeviceObjects(NULL);
GL_ERROR_CHECK
		if(simulPrecipitationRenderer)
			simulPrecipitationRenderer->RestoreDeviceObjects(NULL);
GL_ERROR_CHECK
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->RestoreDeviceObjects(NULL);
GL_ERROR_CHECK
		if(simulCloudRenderer)
			simulCloudRenderer->RestoreDeviceObjects(NULL);
		if(simulLightningRenderer)
			simulLightningRenderer->RestoreDeviceObjects();
	}
	ConnectInterfaces();
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
	InvalidateDeviceObjects();
	operator delete(simulSkyRenderer,memoryInterface);
	operator delete(simulCloudRenderer,memoryInterface);
	operator delete(simul2DCloudRenderer,memoryInterface);
	operator delete(simulLightningRenderer,memoryInterface);
	operator delete(simulPrecipitationRenderer,memoryInterface);
	operator delete(simulAtmosphericsRenderer,memoryInterface);
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

void SimulGLWeatherRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *)
{
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
		i->second->RestoreDeviceObjects(NULL);
	}
GL_ERROR_CHECK
	device_initialized=true;
	EnableCloudLayers();
ERRNO_CHECK
	simulSkyRenderer->RestoreDeviceObjects(NULL);
	simulCloudRenderer->RestoreDeviceObjects(NULL);
	simulLightningRenderer->RestoreDeviceObjects();
ERRNO_CHECK
	simulAtmosphericsRenderer->RestoreDeviceObjects(NULL);
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
	cloud_overlay_program=MakeProgram("simple.vert",NULL,"simul_cloud_overlay.frag");
ERRNO_CHECK
}
void SimulGLWeatherRenderer::InvalidateDeviceObjects()
{
GL_ERROR_CHECK
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	for(FramebufferMap::iterator i=framebuffers.begin();i!=framebuffers.end();i++)
	{
		i->second->InvalidateDeviceObjects();
	}
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
	BaseWeatherRenderer::InvalidateDeviceObjects();
}

void SimulGLWeatherRenderer::ReloadTextures()
{
	BaseWeatherRenderer::ReloadTextures();
}

void SimulGLWeatherRenderer::RecompileShaders()
{
	BaseWeatherRenderer::RecompileShaders();
	SAFE_DELETE_PROGRAM(cloud_overlay_program);
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	cloud_overlay_program=MakeProgram("simple.vert",NULL,"simul_cloud_overlay.frag",defines);
}
#include "Simul/Camera/Camera.h"
void SimulGLWeatherRenderer::RenderSkyAsOverlay(crossplatform::DeviceContext &deviceContext
											,bool is_cubemap
											,float exposure
											,bool buffered
											,crossplatform::Texture *mainDepthTexture
											,const void* lowResDepthTexture
											,const sky::float4& depthViewportXYWH
											,bool doFinalCloudBufferToScreenComposite)
{
	void *context=deviceContext.platform_context;
	RenderCloudsLate=false;
	if(baseSkyRenderer)
	{
		baseSkyRenderer->EnsureTexturesAreUpToDate(context);
		baseSkyRenderer->Render2DFades(deviceContext);
	}
	crossplatform::TwoResFramebuffer *fb			=GetFramebuffer(deviceContext.viewStruct.view_id);
	buffered=(buffered&&fb&&!is_cubemap);
	UpdateSkyAndCloudHookup();
	if(baseAtmosphericsRenderer&&ShowSky)
		baseAtmosphericsRenderer->RenderAsOverlay(deviceContext, mainDepthTexture->AsVoidPointer(),exposure,depthViewportXYWH);
	if(base2DCloudRenderer&&base2DCloudRenderer->GetCloudKeyframer()->GetVisible())
		base2DCloudRenderer->Render(deviceContext,exposure,false,false,mainDepthTexture->AsVoidPointer(),false,depthViewportXYWH,sky::float4(0.f,0.f,1.f,1.f));
	if(buffered)
	{
		fb->GetLowResFarFramebuffer()->Activate(context);
		fb->GetLowResFarFramebuffer()->Clear(context,0.0f,0.0f,0.f,1.f,ReverseDepth?0.f:1.f);
	}
	math::Vector3 cam_pos=simul::camera::GetCameraPosVector(deviceContext.viewStruct.view);
	if(baseSkyRenderer)
	{
		float cloud_occlusion=0;
		if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
			cloud_occlusion=baseCloudRenderer->GetSunOcclusion(cam_pos);
		baseSkyRenderer->CalcSunOcclusion(deviceContext,cloud_occlusion);
	}
	// Do this AFTER sky render, to catch any changes to texture definitions:
	UpdateSkyAndCloudHookup();
	if(baseCloudRenderer&&baseCloudRenderer->GetCloudKeyframer()->GetVisible())
		baseCloudRenderer->Render(deviceContext,buffered?1.f:exposure,is_cubemap,false,mainDepthTexture->AsVoidPointer(),true,depthViewportXYWH,sky::float4(0.f,0.f,1.f,1.f));
	if(buffered)
	{
		fb->GetLowResFarFramebuffer()->Deactivate(context);
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
}
void SimulGLWeatherRenderer::RenderLateCloudLayer(crossplatform::DeviceContext &deviceContext,float exposure,bool
												  ,const simul::sky::float4 &depthViewportXYWH)
{
	if(!(AlwaysRenderCloudsLate||RenderCloudsLate)||!simulCloudRenderer||!simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return;
	crossplatform::TwoResFramebuffer *fb			=GetFramebuffer(deviceContext.viewStruct.view_id);
	fb->GetLowResFarFramebuffer()->Activate(deviceContext.platform_context);
	fb->GetLowResFarFramebuffer()->Clear(deviceContext.platform_context,0,0,0,1.f,ReverseDepth?0.f:1.f);
	simulCloudRenderer->Render(deviceContext,exposure,false,false,NULL,true,depthViewportXYWH,sky::float4(0,0,1.f,1.f));
	fb->GetLowResFarFramebuffer()->Deactivate(deviceContext.platform_context);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glBlendFunc(GL_ONE,GL_SRC_ALPHA);
	GLuint prog=cloud_overlay_program;//AlwaysRenderCloudsLate?cloud_overlay_program:Utilities::GetSingleton().simple_program;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)fb->GetLowResFarFramebuffer()->GetColorTex());
	glUseProgram(prog);

	GLint image_texture		=glGetUniformLocation(prog,"image_texture");
	GLint depthAlphaTexture	=glGetUniformLocation(prog,"depthAlphaTexture");
	glUniform1i(image_texture,0);
	glUniform1i(depthAlphaTexture,1);
	//scene_buffer->Render(context,true);
	glUseProgram(0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void SimulGLWeatherRenderer::RenderLightning(simul::crossplatform::DeviceContext &deviceContext)
{
	math::Matrix4x4 view,proj;
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		simulLightningRenderer->Render(deviceContext,NULL,simul::sky::float4(0,0,1.f,1.f),NULL);
}

void SimulGLWeatherRenderer::RenderPrecipitation(crossplatform::DeviceContext &deviceContext)
{
	math::Matrix4x4 view,proj;
	if(simulPrecipitationRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(deviceContext,NULL,300000.f,sky::float4(0,0,1.f,1.f));
}


class SimulGLSkyRenderer *SimulGLWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer;
}

class SimulGLCloudRenderer *SimulGLWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer;
}

class SimulGL2DCloudRenderer *SimulGLWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}

GLuint SimulGLWeatherRenderer::GetFramebufferTexture(int view_id)
{
	return (GLuint)GetFramebuffer(view_id)->GetLowResFarFramebuffer()->GetColorTex();
}

crossplatform::TwoResFramebuffer *SimulGLWeatherRenderer::GetFramebuffer(int view_id)
{
	if(framebuffers.find(view_id)==framebuffers.end())
	{
		opengl::TwoResFramebuffer *fb=new opengl::TwoResFramebuffer();
		fb->SetDimensions(BufferWidth,BufferHeight,Downscale);
		framebuffers[view_id]=fb;
		fb->RestoreDeviceObjects(NULL);
		return fb;
	}
	return framebuffers[view_id];
}