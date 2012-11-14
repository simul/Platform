#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer()
	:clouds_texture(0)
	,initialized(false)
{
	framebuffer=new FramebufferGL(0,0,GL_TEXTURE_2D,"simul_atmospherics");
}

SimulGLAtmosphericsRenderer::~SimulGLAtmosphericsRenderer()
{
	delete framebuffer;
}

void SimulGLAtmosphericsRenderer::SetBufferSize(int w,int h)
{
	if(w!=framebuffer->GetWidth()||h!=framebuffer->GetHeight())
	{
		framebuffer->SetWidthAndHeight(w,h);
		if(initialized)
			RestoreDeviceObjects(NULL);
	}
}

void SimulGLAtmosphericsRenderer::RestoreDeviceObjects(void *)
{
	initialized=true;
	framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	
  //You can also try GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24 for the internal format.
  //If GL_DEPTH24_STENCIL8_EXT, go ahead and use it (GL_EXT_packed_depth_stencil)
	if(glewIsSupported("GL_EXT_packed_depth_stencil")||IsExtensionSupported("GL_EXT_packed_depth_stencil"))
	{
		framebuffer->InitDepth_RB(GL_DEPTH24_STENCIL8_EXT);
	}
	else
	{
		framebuffer->InitDepth_RB(GL_DEPTH_COMPONENT32);
	}
	RecompileShaders();
}

void SimulGLAtmosphericsRenderer::RecompileShaders()
{
	if(!initialized)
		return;
	framebuffer->RecompileShaders();
	distance_fade_program		=MakeProgram("simul_atmospherics");
	cloudmix_program			=MakeProgram("simul_cloudmix");
ERROR_CHECK
	glUseProgram(distance_fade_program);

	image_texture_param		=glGetUniformLocation(distance_fade_program,"image_texture");
	loss_texture_param		=glGetUniformLocation(distance_fade_program,"loss_texture");
	insc_texture_param		=glGetUniformLocation(distance_fade_program,"insc_texture");
	hazeEccentricity_param	=glGetUniformLocation(distance_fade_program,"hazeEccentricity");
	lightDir_param			=glGetUniformLocation(distance_fade_program,"lightDir");
	invViewProj_param		=glGetUniformLocation(distance_fade_program,"invViewProj");
	mieRayleighRatio_param	=glGetUniformLocation(distance_fade_program,"mieRayleighRatio");

	clouds_texture_param	=glGetUniformLocation(distance_fade_program,"clouds_texture");
	
	glUseProgram(0);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
}


void SimulGLAtmosphericsRenderer::StartRender()
{
	framebuffer->Activate();
	glClearColor(0.f,0.0f,0.0f,1.0f);
	ERROR_CHECK
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	ERROR_CHECK
}

void SimulGLAtmosphericsRenderer::FinishRender()
{
	framebuffer->Deactivate();
ERROR_CHECK
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,loss_texture);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,inscatter_texture);
    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,skylight_texture);
ERROR_CHECK
	glUseProgram(distance_fade_program);
	glUniform1i(image_texture_param,1);
	glUniform1i(loss_texture_param,2);
	glUniform1i(insc_texture_param,3);
	glUniform1i(clouds_texture_param,0);////
	glUniform1f(hazeEccentricity_param,.5f);
	glUniform3f(lightDir_param,.5f,0.5f,0.5f);
ERROR_CHECK
	simul::math::Matrix4x4 view,proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
ERROR_CHECK
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::math::Multiply4x4(viewproj,view,proj);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();
	simul::sky::float4 lightDir		=skyInterface->GetDirectionToLight();
	float hazeEccentricity			=skyInterface->GetMieEccentricity();
ERROR_CHECK
static bool tr=true;
	glUniformMatrix4fv(invViewProj_param,1,tr,ivp.RowPointer(0));
ERROR_CHECK
	glUniform3f(mieRayleighRatio_param,ratio.x,ratio.y,ratio.z);
ERROR_CHECK
	glUniform3f(lightDir_param,lightDir.x,lightDir.y,lightDir.z);
	glUniform1f(hazeEccentricity_param,hazeEccentricity);

	
	glEnable(GL_BLEND);
		// retain background based on alpha in overlay
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,clouds_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,framebuffer->GetColorTex(0));
	
	framebuffer->Render1(distance_fade_program,false);
	glUseProgram(0);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,0);
	ERROR_CHECK
	glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,0);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,0);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,0);
    glDisable(GL_TEXTURE_2D);
	return;
}
