#include <gl/glew.h>
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Platform/OpenGL/Glsl.h"
#include "Simul/Platform/OpenGL/GLSL/simul_earthshadow_uniforms.glsl"

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer()
	:clouds_texture(0)
	,cloud_shadow_texture(0)
	,initialized(false)
	,current_program(0)
	,earthShadowUniformsBindingIndex(1)
	,earthShadowUniforms(0)
	,earthShadowUniformsUBO(0)
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
	std::string defs="";
	distance_fade_program		=MakeProgram("simul_atmospherics",defs.c_str());
	earthshadow_fade_program	=LoadPrograms("simul_atmospherics.vert",NULL,"simul_atmospherics_earthshadow.frag",defs.c_str());
	cloudmix_program			=MakeProgram("simul_cloudmix");
	godrays_program				=LoadPrograms("simul_atmospherics.vert",NULL,"simul_atmospherics_godrays.frag",defs.c_str());
	current_program				=0;
	glUseProgram(0);
	
	glGenBuffers(1, &earthShadowUniformsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, earthShadowUniformsUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(EarthShadowUniforms), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(godrays_program);
	
	earthShadowUniforms=-1;
	earthShadowUniformsUBO=-1;
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
		cloudShadowTexture		=glGetUniformLocation(current_program,"cloudShadowTexture");
		hazeEccentricity		=glGetUniformLocation(current_program,"hazeEccentricity");
		lightDir				=glGetUniformLocation(current_program,"lightDir");
		invViewProj				=glGetUniformLocation(current_program,"invViewProj");
		mieRayleighRatio		=glGetUniformLocation(current_program,"mieRayleighRatio");
		cloudsTexture			=glGetUniformLocation(current_program,"cloudsTexture");
		earthShadowUniforms		=glGetUniformBlockIndex(current_program, "EarthShadowUniforms");
		//directLightMultiplier	=glGetUniformLocation(current_program,"directLightMultiplier");
ERROR_CHECK
		// If that block IS in the shader program, then BIND it to the relevant UBO.
		if(earthShadowUniforms>=0)
		{
			glUniformBlockBinding(current_program,earthShadowUniforms,earthShadowUniformsBindingIndex);
ERROR_CHECK
			glBindBufferRange(GL_UNIFORM_BUFFER, earthShadowUniformsBindingIndex,earthShadowUniformsUBO, 0, sizeof(EarthShadowUniforms));	
		}
ERROR_CHECK
		
		cloudOrigin				=glGetUniformLocation(current_program,"cloudOrigin");
		cloudScale				=glGetUniformLocation(current_program,"cloudScale");
		maxDistance				=glGetUniformLocation(current_program,"maxDistance");
		viewPosition			=glGetUniformLocation(current_program,"viewPosition");
		overcast_param			=glGetUniformLocation(current_program,"overcast");

		printProgramInfoLog(current_program);
ERROR_CHECK
	}
	glUseProgram(p);
}

void SimulGLAtmosphericsRenderer::StartRender()
{
	framebuffer->Activate();
	framebuffer->Clear(0.f,0.0f,1.0f,1.0f);
	ERROR_CHECK
}

void SimulGLAtmosphericsRenderer::FinishRender()
{
	framebuffer->Deactivate();
ERROR_CHECK
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,inscatter_texture);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,cloud_shadow_texture);
    glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,loss_texture);
    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,skylight_texture);
	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 sun_dir		=skyInterface->GetDirectionToSun();
ERROR_CHECK
	simul::sky::EarthShadow e=skyInterface->GetEarthShadow(cam_pos.z/1000.f,skyInterface->GetDirectionToSun());
	if(e.enable)
	{
		EarthShadowUniforms u;
		u.earthShadowNormal	=e.normal;
		u.radiusOnCylinder	=e.radius_on_cylinder;
		u.maxFadeDistance	=fade_distance_km/e.planet_radius;
		u.terminatorCosine	=e.terminator_cosine;
		u.sunDir			=sun_dir;
		glBindBuffer(GL_UNIFORM_BUFFER, earthShadowUniformsUBO);
		glBufferSubData(GL_UNIFORM_BUFFER,0, sizeof(EarthShadowUniforms), &u);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	if(e.enable)
		UseProgram(earthshadow_fade_program);
	else
		UseProgram(distance_fade_program);
	ERROR_CHECK
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();
	glUniform3f(mieRayleighRatio,ratio.x,ratio.y,ratio.z);
	glUniform3f(lightDir,light_dir.x,light_dir.y,light_dir.z);
	glUniform1f(hazeEccentricity,skyInterface->GetMieEccentricity());
	
	glUniform1i(imageTexture,0);
	glUniform1i(inscTexture,1);
	glUniform1i(cloudShadowTexture,2);
	glUniform1i(lossTexture,3);
	glUniform1i(skylightTexture,4);
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
	setParameter3(cloudOrigin,cloud_origin);
	setParameter3(cloudScale,cloud_scale);
	setParameter(maxDistance,fade_distance_km*1000.f);
	setParameter3(viewPosition,cam_pos);
	setParameter(overcast_param,overcast);
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
	glUniformMatrix4fv(invViewProj,1,true,ivp.RowPointer(0));
ERROR_CHECK

	//glUniform1f(directLightMultiplier,e.illumination);
	
	glEnable(GL_BLEND);
	// retain background based on alpha in overlay
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,framebuffer->GetColorTex(0));
	
	framebuffer->Render(false);
	
	if(ShowGodrays)
	{
	// Draw the godrays over the entire scene - or to be more accurate, subtract the cloud shadows from the scene.
		glUseProgram(godrays_program);
		setParameter(godrays_program	,"imageTexture"		,0);
		setParameter(godrays_program	,"inscTexture"		,1);
		setParameter(godrays_program	,"cloudShadowTexture",2);
		setParameter3(godrays_program	,"mieRayleighRatio",ratio);
		setParameter3(godrays_program	,"lightDir"			,light_dir);
		setParameter(godrays_program	,"hazeEccentricity"	,skyInterface->GetMieEccentricity());
		setMatrix(godrays_program		,"invViewProj"		,ivp.RowPointer(0));
		setParameter3(godrays_program	,"cloudOrigin"		,cloud_origin);
		setParameter3(godrays_program	,"cloudScale"		,cloud_scale);
		setParameter(godrays_program	,"maxDistance"		,fade_distance_km*1000.f);
		setParameter3(godrays_program	,"viewPosition"		,cam_pos);
		glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);//GL_FUNC_SUBTRACT);
		glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ONE);
		framebuffer->Render(false);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);
	}
	
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
ERROR_CHECK
}
