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
	,current_program(0)
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
	distance_fade_program		=MakeProgram("simul_atmospherics");
	earthshadow_fade_program	=LoadPrograms("simul_atmospherics.vert",NULL,"simul_atmospherics_earthshadow.frag");
	cloudmix_program			=MakeProgram("simul_cloudmix");
	current_program				=0;
	glUseProgram(0);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
}

void SimulGLAtmosphericsRenderer::UseProgram(GLuint p)
{
	if(p&&p!=current_program)
	{
		current_program=p;
ERROR_CHECK
		glUseProgram(current_program);
		imageTexture			=glGetUniformLocation(current_program,"imageTexture");
		lossTexture				=glGetUniformLocation(current_program,"lossTexture");
		inscTexture				=glGetUniformLocation(current_program,"inscTexture");
		skylightTexture			=glGetUniformLocation(current_program,"skylightTexture");
		hazeEccentricity		=glGetUniformLocation(current_program,"hazeEccentricity");
		lightDir				=glGetUniformLocation(current_program,"lightDir");
		invViewProj				=glGetUniformLocation(current_program,"invViewProj");
		mieRayleighRatio		=glGetUniformLocation(current_program,"mieRayleighRatio");
		cloudsTexture			=glGetUniformLocation(current_program,"cloudsTexture");
		directLightMultiplier	=glGetUniformLocation(current_program,"directLightMultiplier");
		earthShadowNormal		=glGetUniformLocation(current_program,"earthShadowNormal");
		maxFadeDistance			=glGetUniformLocation(current_program,"maxFadeDistance");
		radiusOnCylinder		=glGetUniformLocation(current_program,"radiusOnCylinder");
		terminatorCosine		=glGetUniformLocation(current_program,"terminatorCosine");
		printProgramInfoLog(current_program);
ERROR_CHECK
	}
	glUseProgram(p);
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
	if(skyInterface)
	{
		simul::sky::float4 sun_dir		=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
ERROR_CHECK
		simul::sky::EarthShadow e=skyInterface->GetEarthShadow(cam_pos.z/1000.f,skyInterface->GetDirectionToSun());
		if(e.enable)
			UseProgram(earthshadow_fade_program);
		else
			UseProgram(distance_fade_program);
	ERROR_CHECK
		simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();
		glUniform3f(mieRayleighRatio,ratio.x,ratio.y,ratio.z);
		glUniform3f(lightDir,sun_dir.x,sun_dir.y,sun_dir.z);
		glUniform1f(hazeEccentricity,skyInterface->GetMieEccentricity());
		if(current_program==earthshadow_fade_program)
		{
			glUniform1f(radiusOnCylinder,e.radius_on_cylinder);
			glUniform3f(earthShadowNormal,e.normal.x,e.normal.y,e.normal.z);
			glUniform1f(maxFadeDistance,fade_distance_km/e.planet_radius);
			glUniform1f(terminatorCosine,e.terminator_cosine);
		}
	}
	else
		UseProgram(distance_fade_program);
	glUniform1i(cloudsTexture,0);
	glUniform1i(imageTexture,1);
	glUniform1i(lossTexture,2);
	glUniform1i(inscTexture,3);
	glUniform1i(skylightTexture,4);
ERROR_CHECK
	simul::math::Matrix4x4 view,proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
ERROR_CHECK
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	CalcCameraPosition(cam_pos);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::math::Multiply4x4(viewproj,view,proj);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
ERROR_CHECK
static bool tr=true;
	glUniformMatrix4fv(invViewProj,1,tr,ivp.RowPointer(0));
ERROR_CHECK

	//glUniform1f(directLightMultiplier,e.illumination);
	
	glEnable(GL_BLEND);
	// retain background based on alpha in overlay
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,clouds_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,framebuffer->GetColorTex(0));
	
	framebuffer->Render(false);
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
