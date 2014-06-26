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
#include "Simul/Platform/OpenGL/Effect.h"
#include <stdint.h>  // for uintptr_t
#include <string.h>  // for memset

using namespace simul;
using namespace opengl;

class LossBlendState:public opengl::PassState
{
public:
	void Apply()
	{
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
	}
};
static LossBlendState lossBlendState;
class InscBlendState:public opengl::PassState
{
public:
	void Apply()
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
		glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
	}
};
static InscBlendState inscBlendState;
SimulGLAtmosphericsRenderer::SimulGLAtmosphericsRenderer(simul::base::MemoryInterface *m)
	:BaseAtmosphericsRenderer(m)
	,clouds_texture(0)
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
	RecompileShaders();
}

void SimulGLAtmosphericsRenderer::RecompileShaders()
{
	BaseAtmosphericsRenderer::RecompileShaders();
	crossplatform::EffectTechniqueGroup *group=effect->GetTechniqueGroupByName("atmospherics_overlay");
	if(group)
	{
		for(int i=0;i<(int)group->techniques.size();i++)
		{
			opengl::EffectTechnique *t=(opengl::EffectTechnique *)group->GetTechniqueByIndex(i);
			if(t)
			{
				t->passStates[t->GetPassIndex("loss")]=&lossBlendState;
				t->passStates[t->GetPassIndex("inscatter")]=&inscBlendState;
			}
		}
	}
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
	
	effect->Apply(deviceContext,tech,"loss");
	
	glEnable(GL_BLEND);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);

	effect->Apply(deviceContext,tech,"inscatter");
	atmosphericsUniforms.Apply(deviceContext);
	effect->SetTexture("inscTexture"		,overcInscTexture);
	effect->SetTexture("skylightTexture"	,skylightTexture);
	effect->SetTexture("depthTexture"		,depthTexture);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
	renderPlatform->DrawQuad(deviceContext);
	effect->SetTexture("depthTextureMS"	,NULL);
	effect->SetTexture("depthTexture"	,NULL);
	effect->SetTexture("lossTexture",NULL);
	effect->SetTexture("inscTexture",NULL);
	effect->SetTexture("skylTexture",NULL);
	effect->SetTexture("illuminationTexture",NULL);
	effect->SetTexture("cloudShadowTexture",NULL);
	atmosphericsPerViewConstants.Unbind(deviceContext);
	atmosphericsUniforms.Unbind(deviceContext);
	earthShadowUniforms.Unbind(deviceContext);
	effect->Unapply(deviceContext);
	glDisable(GL_BLEND);
GL_ERROR_CHECK
}
