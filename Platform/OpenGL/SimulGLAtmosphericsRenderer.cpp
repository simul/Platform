#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer()
	:max_fade_distance_metres(200000.f)
{
	framebuffer=new FramebufferGL(0,0,GL_TEXTURE_2D);
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
		RestoreDeviceObjects(NULL);
	}
}

bool SimulGLAtmosphericsRenderer::RestoreDeviceObjects(void *)
{
	framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	ReloadShaders();
	return true;
}

void SimulGLAtmosphericsRenderer::ReloadShaders()
{
	framebuffer->ReloadShaders();
	glUseProgram(framebuffer->GetProgram());
	image_texture_param		=glGetUniformLocation(framebuffer->GetProgram(),"image_texture");
	loss_texture_param		=glGetUniformLocation(framebuffer->GetProgram(),"loss_texture");
	insc_texture_param		=glGetUniformLocation(framebuffer->GetProgram(),"insc_texture");
	hazeEccentricity_param	=glGetUniformLocation(framebuffer->GetProgram(),"hazeEccentricity");
	lightDir_param			=glGetUniformLocation(framebuffer->GetProgram(),"lightDir");
	invViewProj_param		=glGetUniformLocation(framebuffer->GetProgram(),"invViewProj");
	mieRayleighRatio_param	=glGetUniformLocation(framebuffer->GetProgram(),"mieRayleighRatio");
	glUseProgram(0);
}

bool SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	return true;
}

void SimulGLAtmosphericsRenderer::SetMaxFadeDistanceKm(float dist_km)
{
	max_fade_distance_metres=dist_km*1000.f;
}

void SimulGLAtmosphericsRenderer::StartRender()
{
	framebuffer->Activate();
	glClearColor(0.f,0.f,0.f,1.f);
	ERROR_CHECK
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	ERROR_CHECK
}

void SimulGLAtmosphericsRenderer::FinishRender()
{
ERROR_CHECK
    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,loss_texture);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,inscatter_texture);
ERROR_CHECK
	glUseProgram(framebuffer->GetProgram());
	glUniform1i(image_texture_param,0);
	glUniform1i(loss_texture_param,1);
	glUniform1i(insc_texture_param,2);
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
	simul::sky::float4 ratio=skyInterface->GetMieRayleighRatio();
	simul::sky::float4 lightDir=skyInterface->GetDirectionToLight();
	float hazeEccentricity=skyInterface->GetMieEccentricity();
ERROR_CHECK
static bool tr=true;
	glUniformMatrix4fv(invViewProj_param,1,tr,ivp.RowPointer(0));
ERROR_CHECK
	glUniform3f(mieRayleighRatio_param,ratio.x,ratio.y,ratio.z);
ERROR_CHECK
	glUniform3f(lightDir_param,lightDir.x,lightDir.y,lightDir.z);
	glUniform1f(hazeEccentricity_param,hazeEccentricity);

	framebuffer->DeactivateAndRender(true);
	glUseProgram(0);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,0);
	ERROR_CHECK
}
