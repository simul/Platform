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
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
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
}

SimulGLAtmosphericsRenderer::~SimulGLAtmosphericsRenderer()
{
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

void SimulGLAtmosphericsRenderer::InvalidateDeviceObjects()
{
	BaseAtmosphericsRenderer::InvalidateDeviceObjects();
}

void SimulGLAtmosphericsRenderer::RenderAsOverlay(crossplatform::DeviceContext &deviceContext
												  ,crossplatform::Texture *depthTexture
												  ,float exposure
												  ,const simul::sky::float4& depthViewportXYWH)
{
	bool msaa=false;
	if(depthTexture)
	{
		msaa=(depthTexture->GetSampleCount()>0);
	}
	if(msaa)
		effect->SetTexture("depthTextureMS"		,depthTexture);
	else
		effect->SetTexture("depthTexture"		,depthTexture);

	effect->SetTexture("lossTexture",skyLossTexture);
	effect->SetTexture("inscTexture",overcInscTexture);
	effect->SetTexture("skylTexture",skylightTexture);
	
	effect->SetTexture("illuminationTexture",illuminationTexture);
	effect->SetTexture("cloudShadowTexture",cloudShadowStruct.texture);

	SetAtmosphericsPerViewConstants(atmosphericsPerViewConstants,exposure,deviceContext.viewStruct.view,deviceContext.viewStruct.proj,deviceContext.viewStruct.proj,depthViewportXYWH);
	atmosphericsPerViewConstants.Apply(deviceContext);
	
	SetAtmosphericsConstants(atmosphericsUniforms,simul::sky::float4(1.0,1.0,1.0,0.0));
	atmosphericsUniforms.Apply(deviceContext);
	
	crossplatform::EffectTechniqueGroup *group=effect->GetTechniqueGroupByName("atmospherics_overlay");
	crossplatform::EffectTechnique *tech=group->GetTechniqueByName(msaa?"msaa":"standard");
	
	effect->Apply(deviceContext,tech,0);
	effect->SetTexture("lossTexture",skyLossTexture);
	effect->SetTexture("depthTexture",depthTexture);
	glEnable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);

//	GLuint current_insc_program=effect->GetTechniqueByName("inscatter")->passAsGLuint(0);
	effect->Apply(deviceContext,effect->GetTechniqueByName("inscatter"),0);//current_insc_program);
	atmosphericsUniforms.Apply(deviceContext);
	effect->SetTexture("inscTexture"	,overcInscTexture);
	effect->SetTexture("skylightTexture"	,skylightTexture);
	effect->SetTexture("depthTexture"		,depthTexture);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
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
