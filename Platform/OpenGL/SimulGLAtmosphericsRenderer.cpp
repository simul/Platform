#include <GL/glew.h>
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "Simul/Platform/CrossPlatform/SL/atmospherics_constants.sl"
#include "Simul/Camera/Camera.h"
#include <stdint.h>  // for uintptr_t
#include <string.h>  // for memset

using namespace simul;
using namespace opengl;

SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,clouds_texture(0)
	,initialized(false)
	,input_texture(0)
	,depth_texture(0)
{
	//framebuffer=new FramebufferGL(0,0,GL_TEXTURE_2D);
}

SimulGLAtmosphericsRenderer::~SimulGLAtmosphericsRenderer()
{
	//delete framebuffer;
}

void SimulGLAtmosphericsRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseAtmosphericsRenderer::RestoreDeviceObjects(r);
	initialized=true;
	RecompileShaders();
}

static GLint earthShadowUniformsBindingIndex=3;
static GLint atmosphericsUniformsBindingIndex=11;
static GLint atmosphericsUniforms2BindingIndex=12;

void SimulGLAtmosphericsRenderer::RecompileShaders()
{
	if(!initialized)
		return;
	BaseAtmosphericsRenderer::RecompileShaders();
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]	=ReverseDepth?"1":"0";
	
	
	glUseProgram(0);
}

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	BaseAtmosphericsRenderer::InvalidateDeviceObjects();
}

void SimulGLAtmosphericsRenderer::RenderAsOverlay(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& depthViewportXYWH)
{
	GLuint depth_texture=depthTexture->AsGLuint();
GL_ERROR_CHECK
    glEnable(GL_TEXTURE_2D);
GL_ERROR_CHECK
	simul::math::Matrix4x4 view,proj;
	simul::sky::float4 cam_pos;
	CalcCameraPosition(cam_pos);
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix((const float*)proj);

	simul::sky::float4 light_dir	=skyInterface->GetDirectionToLight(cam_pos.z/1000.f);
	simul::sky::float4 sun_dir		=skyInterface->GetDirectionToSun();
GL_ERROR_CHECK
	simul::sky::EarthShadow e=skyInterface->GetEarthShadow(cam_pos.z/1000.f,skyInterface->GetDirectionToSun());
	if(e.enable)
	{
		earthShadowUniforms.earthShadowNormal	=e.normal;
		earthShadowUniforms.radiusOnCylinder	=e.radius_on_cylinder;
		earthShadowUniforms.maxFadeDistance		=fade_distance_km/e.planet_radius;
		earthShadowUniforms.terminatorDistance	=e.terminator_distance_km/fade_distance_km;
		earthShadowUniforms.sunDir				=sun_dir;
		earthShadowUniforms.Apply(deviceContext);
//		UPDATE_GL_CONSTANT_BUFFER(earthShadowUniformsUBO,earthShadowUniforms,earthShadowUniformsBindingIndex)
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
	
	atmosphericsPerViewConstants.invViewProj=ivp;
	atmosphericsPerViewConstants.invViewProj.transpose();
	atmosphericsPerViewConstants.tanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);
	atmosphericsPerViewConstants.nearZ=frustum.nearZ*0.001f/fade_distance_km;
	atmosphericsPerViewConstants.farZ=frustum.farZ*0.001f/fade_distance_km;
	
	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,view,proj,proj,depthViewportXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	
GL_ERROR_CHECK
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	
GL_ERROR_CHECK
	glEnable(GL_BLEND);

	glUseProgram(effect->GetTechniqueByName("loss")->passAsGLuint(0));
	setTexture(effect->GetTechniqueByName("loss")->passAsGLuint(0),"lossTexture",3,skyLossTexture->AsGLuint());
	setTexture(effect->GetTechniqueByName("loss")->passAsGLuint(0),"depthTexture",5,depth_texture);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
	DrawQuad(0.f,0.f,1.f,1.f);

	GLuint current_insc_program=effect->GetTechniqueByName(e.enable?"earthshadow_inscatter":"inscatter")->passAsGLuint(0);
	glUseProgram(current_insc_program);
	setTexture(current_insc_program,"inscTexture"		,1,overcInscTexture->AsGLuint());
	setTexture(current_insc_program,"skylightTexture"	,4,skylightTexture->AsGLuint());
	setTexture(current_insc_program,"depthTexture"		,5,depth_texture);
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
