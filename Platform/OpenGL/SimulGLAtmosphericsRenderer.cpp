#include <gl/glew.h>
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/earth_shadow_uniforms.sl"
#include "Simul/Platform/CrossPlatform/atmospherics_constants.sl"
#include <stdint.h>  // for uintptr_t
#include <string.h>  // for memset
using namespace simul;
using namespace opengl;

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,clouds_texture(0)
	,initialized(false)
	,insc_program(0)
	,loss_program(0)
	,earthshadow_insc_program(0)
	,godrays_program(0)
	,earthShadowUniformsUBO(0)
	,atmosphericsUniformsUBO(0)
	,atmosphericsUniforms2UBO(0)
	,loss_texture(0)
	,inscatter_texture(0)
	,skylight_texture(0)
	,overcast_texture(0)
	,input_texture(0)
	,depth_texture(0)
{
	//framebuffer=new FramebufferGL(0,0,GL_TEXTURE_2D);
}

SimulGLAtmosphericsRenderer::~SimulGLAtmosphericsRenderer()
{
	//delete framebuffer;
}

void SimulGLAtmosphericsRenderer::RestoreDeviceObjects(void *)
{
	initialized=true;
	//framebuffer->InitColor_Tex(0,GL_RGBA32F_ARB);
	
  //You can also try GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24 for the internal format.
  //If GL_DEPTH24_STENCIL8_EXT, go ahead and use it (GL_EXT_packed_depth_stencil)
/*	if(glewIsSupported("GL_EXT_packed_depth_stencil")||IsExtensionSupported("GL_EXT_packed_depth_stencil"))
	{
		framebuffer->InitDepth_RB(GL_DEPTH24_STENCIL8_EXT);
	}
	else
	{
		framebuffer->InitDepth_RB(GL_DEPTH_COMPONENT32);
	}
	framebuffer->NoDepth();*/
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
	godrays_program				=MakeProgram("simul_atmospherics.vert",NULL,"simul_atmospherics_godrays.frag",defines);

	MAKE_GL_CONSTANT_BUFFER(earthShadowUniformsUBO,EarthShadowUniforms,earthShadowUniformsBindingIndex);
	MAKE_GL_CONSTANT_BUFFER(atmosphericsUniformsUBO,AtmosphericsUniforms,atmosphericsUniformsBindingIndex);
	MAKE_GL_CONSTANT_BUFFER(atmosphericsUniforms2UBO,AtmosphericsPerViewConstants,atmosphericsUniforms2BindingIndex);
	
	linkToConstantBuffer(loss_program,"AtmosphericsUniforms",atmosphericsUniformsBindingIndex);
	linkToConstantBuffer(loss_program,"AtmosphericsPerViewConstants",atmosphericsUniforms2BindingIndex);
	
	linkToConstantBuffer(insc_program,"AtmosphericsUniforms",atmosphericsUniformsBindingIndex);
	linkToConstantBuffer(insc_program,"AtmosphericsPerViewConstants",atmosphericsUniforms2BindingIndex);
	
	linkToConstantBuffer(earthshadow_insc_program,"AtmosphericsUniforms",atmosphericsUniformsBindingIndex);
	linkToConstantBuffer(earthshadow_insc_program,"AtmosphericsPerViewConstants",atmosphericsUniforms2BindingIndex);
	linkToConstantBuffer(earthshadow_insc_program,"EarthShadowUniforms",earthShadowUniformsBindingIndex);

	glUseProgram(0);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(godrays_program);

	SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(earthshadow_insc_program);
}

#include "Simul/Camera/Camera.h"

void SimulGLAtmosphericsRenderer::RenderAsOverlay(void *,const void *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH)
{
	GLuint depth_texture=(GLuint)(uintptr_t)depthTexture;
GL_ERROR_CHECK
    glEnable(GL_TEXTURE_2D);
GL_ERROR_CHECK
	simul::math::Matrix4x4 view,proj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 sun_dir		=skyInterface->GetDirectionToSun();
GL_ERROR_CHECK
	simul::sky::EarthShadow e=skyInterface->GetEarthShadow(cam_pos.z/1000.f,skyInterface->GetDirectionToSun());
	if(e.enable)
	{
		EarthShadowUniforms earthShadowUniforms;
		memset(&earthShadowUniforms,0,sizeof(earthShadowUniforms));
		earthShadowUniforms.earthShadowNormal	=e.normal;
		earthShadowUniforms.radiusOnCylinder	=e.radius_on_cylinder;
		earthShadowUniforms.maxFadeDistance		=fade_distance_km/e.planet_radius;
		earthShadowUniforms.terminatorDistance	=e.terminator_distance_km/fade_distance_km;
		earthShadowUniforms.sunDir				=sun_dir;
		UPDATE_GL_CONSTANT_BUFFER(earthShadowUniformsUBO,earthShadowUniforms,earthShadowUniformsBindingIndex)
	}
	GL_ERROR_CHECK
	simul::sky::float4 ratio		=skyInterface->GetMieRayleighRatio();

GL_ERROR_CHECK
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	CalcCameraPosition(cam_pos);
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::math::Multiply4x4(viewproj,view,proj);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 ivp;
	vpt.Inverse(ivp);
	
	AtmosphericsPerViewConstants atmosphericsUniforms2;
	atmosphericsUniforms2.invViewProj=ivp;
	atmosphericsUniforms2.invViewProj.transpose();
	atmosphericsUniforms2.tanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	atmosphericsUniforms2.nearZ=frustum.nearZ*0.001f/fade_distance_km;
	atmosphericsUniforms2.farZ=frustum.farZ*0.001f/fade_distance_km;
	atmosphericsUniforms2.viewPosition	=cam_pos;

	UPDATE_GL_CONSTANT_BUFFER(atmosphericsUniforms2UBO,atmosphericsUniforms2,atmosphericsUniforms2BindingIndex)

	{
		AtmosphericsUniforms atmosphericsUniforms;
		float alt_km			=cam_pos.z/1000.f;
		atmosphericsUniforms.lightDir			=(const float*)skyInterface->GetDirectionToLight(alt_km);
		atmosphericsUniforms.mieRayleighRatio	=(const float*)skyInterface->GetMieRayleighRatio();
		atmosphericsUniforms.texelOffsets		=vec2(0,0);
		atmosphericsUniforms.hazeEccentricity	=skyInterface->GetMieEccentricity();
		
		atmosphericsUniforms.cloudOrigin	=cloud_origin;
		atmosphericsUniforms.cloudScale		=cloud_scale;
		atmosphericsUniforms.maxFadeDistanceMetres	=fade_distance_km*1000.f;
		atmosphericsUniforms.overcast		=overcast;
		atmosphericsUniforms.exposure		=exposure;
		
		UPDATE_GL_CONSTANT_BUFFER(atmosphericsUniformsUBO,atmosphericsUniforms,atmosphericsUniformsBindingIndex)
	}
GL_ERROR_CHECK
	glEnable(GL_BLEND);

	glUseProgram(loss_program);
	setTexture(loss_program,"lossTexture",3,loss_texture);
	setTexture(loss_program,"depthTexture",5,depth_texture);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
	DrawQuad(0.f,0.f,1.f,1.f);

	GLuint current_insc_program=e.enable?earthshadow_insc_program:insc_program;
	glUseProgram(current_insc_program);
	setTexture(current_insc_program,"inscTexture"		,1,overcast_texture);
	//setTexture(current_insc_program,"cloudShadowTexture",2,cloud_shadow_texture);
	setTexture(current_insc_program,"skylightTexture"	,4,skylight_texture);
	setTexture(current_insc_program,"depthTexture"		,5,depth_texture);
	// retain background based on alpha in overlay
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
	DrawQuad(0.f,0.f,1.f,1.f);
	
	glUseProgram(0);
    glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,0);
	GL_ERROR_CHECK
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
GL_ERROR_CHECK
}
