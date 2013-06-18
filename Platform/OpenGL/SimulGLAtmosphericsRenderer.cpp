#include <gl/glew.h>
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/OpenGL/GLSL/simul_earthshadow_uniforms.glsl"
#include "Simul/Platform/CrossPlatform/atmospherics_constants.sl"

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer()
	:clouds_texture(0)
	,cloud_shadow_texture(0)
	,initialized(false)
	,insc_program(0)
	,loss_program(0)
	,earthshadow_insc_program(0)
	,godrays_program(0)
	,current_insc_program(0)
	,cloudmix_program(0)
	,earthShadowUniformsUBO(0)
	,atmosphericsUniformsUBO(0)
	,atmosphericsUniforms2UBO(0)
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
		if(initialized)
			RestoreDeviceObjects(NULL);
	}
}

void SimulGLAtmosphericsRenderer::RestoreDeviceObjects(void *)
{
	initialized=true;
	framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB);
	
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
	framebuffer->NoDepth();
	RecompileShaders();
}

static GLint earthShadowUniformsBindingIndex=3;
static GLint atmosphericsUniformsBindingIndex=11;
static GLint atmosphericsUniforms2BindingIndex=12;

void SimulGLAtmosphericsRenderer::RecompileShaders()
{
	if(!initialized)
		return;
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	loss_program				=MakeProgram("simul_atmospherics.vert",NULL,"simul_loss.frag",defines);
	insc_program				=MakeProgram("simul_atmospherics.vert",NULL,"simul_insc.frag",defines);
	earthshadow_insc_program	=MakeProgram("simul_atmospherics.vert",NULL,"simul_insc_earthshadow.frag",defines);
	cloudmix_program			=MakeProgram("simul_cloudmix");
	godrays_program				=MakeProgram("simul_atmospherics.vert",NULL,"simul_atmospherics_godrays.frag",defines);
	current_insc_program		=0;
	glUseProgram(0);
	MAKE_CONSTANT_BUFFER(earthShadowUniformsUBO,EarthShadowUniforms,earthShadowUniformsBindingIndex);
	MAKE_CONSTANT_BUFFER(atmosphericsUniformsUBO,AtmosphericsUniforms,atmosphericsUniformsBindingIndex);
	MAKE_CONSTANT_BUFFER(atmosphericsUniforms2UBO,AtmosphericsUniforms2,atmosphericsUniforms2BindingIndex);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(godrays_program);

	SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(earthshadow_insc_program);
	SAFE_DELETE_PROGRAM(cloudmix_program);

}

void SimulGLAtmosphericsRenderer::UseProgram(GLuint p)
{
	if(p&&p!=current_insc_program)
	{
		current_insc_program=p;
ERROR_CHECK
		glUseProgram(p);
		imageTexture			=glGetUniformLocation(p,"imageTexture");
		lossTexture				=glGetUniformLocation(p,"lossTexture");
		inscTexture				=glGetUniformLocation(p,"inscTexture");
		skylightTexture			=glGetUniformLocation(p,"skylightTexture");
		cloudShadowTexture		=glGetUniformLocation(p,"cloudShadowTexture");
		hazeEccentricity		=glGetUniformLocation(p,"hazeEccentricity");
		lightDir				=glGetUniformLocation(p,"lightDir");
		invViewProj				=glGetUniformLocation(p,"invViewProj");
		mieRayleighRatio		=glGetUniformLocation(p,"mieRayleighRatio");
		cloudsTexture			=glGetUniformLocation(p,"cloudsTexture");
	
		GLint earthShadowUniforms	=glGetUniformBlockIndex(p,"EarthShadowUniforms");
		GLint atmosphericsUniforms	=glGetUniformBlockIndex(p,"AtmosphericsUniforms");
		GLint atmosphericsUniforms2	=glGetUniformBlockIndex(p,"AtmosphericsUniforms2");
ERROR_CHECK
		// If that block IS in the shader program, then BIND it to the relevant UBO.
		if(earthShadowUniforms>=0)
			glUniformBlockBinding(p,earthShadowUniforms,earthShadowUniformsBindingIndex);
		if(atmosphericsUniforms>=0)
			glUniformBlockBinding(p,atmosphericsUniforms,atmosphericsUniformsBindingIndex);
		if(atmosphericsUniforms2>=0)
			glUniformBlockBinding(p,atmosphericsUniforms2,atmosphericsUniforms2BindingIndex);
ERROR_CHECK
ERROR_CHECK
		
		cloudOrigin				=glGetUniformLocation(current_insc_program,"cloudOrigin");
		cloudScale				=glGetUniformLocation(current_insc_program,"cloudScale");
		maxDistance				=glGetUniformLocation(current_insc_program,"maxDistance");
		viewPosition			=glGetUniformLocation(current_insc_program,"viewPosition");
		overcast_param			=glGetUniformLocation(current_insc_program,"overcast");

ERROR_CHECK
	}
	glUseProgram(p);
	//if(earthShadowUniforms>=0)
	//	glUniformBlockBinding(p,earthShadowUniforms,earthShadowUniformsBindingIndex);
}

#include "Simul/Camera/Camera.h"

void SimulGLAtmosphericsRenderer::RenderAsOverlay(void *context,const void *depthTexture,float exposure)
{
	GLuint depth_texture=(GLuint)depthTexture;
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
ERROR_CHECK
	simul::math::Matrix4x4 view,proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	
	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 sun_dir		=skyInterface->GetDirectionToSun();
ERROR_CHECK
	simul::sky::EarthShadow e=skyInterface->GetEarthShadow(cam_pos.z/1000.f,skyInterface->GetDirectionToSun());
	if(e.enable)
		UseProgram(earthshadow_insc_program);
	else
		UseProgram(insc_program);
	if(e.enable)
	{
		EarthShadowUniforms u;
		u.earthShadowNormal	=e.normal;
		u.radiusOnCylinder	=e.radius_on_cylinder;
		u.maxFadeDistance	=fade_distance_km/e.planet_radius;
		u.terminatorCosine	=e.terminator_cosine;
		u.sunDir			=sun_dir;
		/*glBindBuffer(GL_UNIFORM_BUFFER, earthShadowUniformsUBO);
		glBufferSubData(GL_UNIFORM_BUFFER,0, sizeof(EarthShadowUniforms), &u);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);*/
ERROR_CHECK
		setParameter3(earthshadow_insc_program,"sunDir",sun_dir);
		setParameter3(earthshadow_insc_program,"earthShadowNormal",e.normal);
		setParameter(earthshadow_insc_program,"radiusOnCylinder",u.radiusOnCylinder);
		setParameter(earthshadow_insc_program,"maxFadeDistance",u.maxFadeDistance);
		setParameter(earthshadow_insc_program,"terminatorCosine",u.terminatorCosine);
	}
	ERROR_CHECK
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();
	glUniform3f(mieRayleighRatio,ratio.x,ratio.y,ratio.z);
	glUniform3f(lightDir,light_dir.x,light_dir.y,light_dir.z);
	glUniform1f(hazeEccentricity,skyInterface->GetMieEccentricity());
	
	glUniform1i(imageTexture,0);
	glUniform1i(inscTexture,1);
	glUniform1i(cloudShadowTexture,2);
	//setTexture(current_insc_program,"lossTexture",3,loss_texture);
	setTexture(current_insc_program,"skylightTexture",4,skylight_texture);
	setTexture(current_insc_program,"depthTexture",5,depth_texture);
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
	setParameter3(cloudOrigin,cloud_origin);
	setParameter3(cloudScale,cloud_scale);
	setParameter(maxDistance,fade_distance_km*1000.f);
	setParameter3(viewPosition,cam_pos);
	setParameter(overcast_param,overcast);
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
	
	AtmosphericsUniforms2 atmosphericsUniforms2;
	atmosphericsUniforms2.invViewProj=ivp;
	atmosphericsUniforms2.invViewProj.transpose();
	atmosphericsUniforms2.tanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	atmosphericsUniforms2.nearZ=frustum.nearZ*0.001f/fade_distance_km;
	atmosphericsUniforms2.farZ=frustum.farZ*0.001f/fade_distance_km;

	UPDATE_CONSTANT_BUFFER(atmosphericsUniforms2UBO,atmosphericsUniforms2,atmosphericsUniforms2BindingIndex)

	{
		AtmosphericsUniforms atmosphericsUniforms;
		float alt_km			=cam_pos.z/1000.f;
		atmosphericsUniforms.lightDir			=(const float*)skyInterface->GetDirectionToLight(alt_km);
		atmosphericsUniforms.mieRayleighRatio	=(const float*)skyInterface->GetMieRayleighRatio();
		atmosphericsUniforms.texelOffsets		=vec2(0,0);
		atmosphericsUniforms.hazeEccentricity	=skyInterface->GetMieEccentricity();
		UPDATE_CONSTANT_BUFFER(atmosphericsUniformsUBO,atmosphericsUniforms,atmosphericsUniformsBindingIndex)
	}
ERROR_CHECK
	//glUniform1f(directLightMultiplier,e.illumination);
	glEnable(GL_BLEND);
	// retain background based on alpha in overlay
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);

    //glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D,(GLuint)framebuffer->GetColorTex());
	
	DrawQuad(0.f,0.f,1.f,1.f);
	
	if(ShowGodrays)
	{
	// Draw the godrays over the entire scene - or to be more accurate, subtract the cloud shadows from the scene.
		glUseProgram(godrays_program);
		setParameter(godrays_program		,"imageTexture"		,0);
		setParameter(godrays_program		,"inscTexture"		,1);
		setParameter(godrays_program		,"cloudShadowTexture",2);
		setParameter3(godrays_program		,"mieRayleighRatio",ratio);
		setParameter3(godrays_program		,"lightDir"			,light_dir);
		setParameter(godrays_program		,"hazeEccentricity"	,skyInterface->GetMieEccentricity());
		setMatrixTranspose(godrays_program	,"invViewProj"		,ivp.RowPointer(0));
		setParameter3(godrays_program		,"cloudOrigin"		,cloud_origin);
		setParameter3(godrays_program		,"cloudScale"		,cloud_scale);
		setParameter(godrays_program		,"maxDistance"		,fade_distance_km*1000.f);
		setParameter3(godrays_program		,"viewPosition"		,cam_pos);
		glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);//GL_FUNC_SUBTRACT);
		glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ONE);
		framebuffer->Render(context,false);
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
    glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,0);
    glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D,0);
    glDisable(GL_TEXTURE_2D);
ERROR_CHECK
}
