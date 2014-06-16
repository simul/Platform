#include <GL/glew.h>
#include <GL/glfx.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

using namespace simul;
using namespace opengl;

#define CHECK_PARAM_EXISTS\
	if(loc<0)\
	{\
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLFX program "<<filename.c_str()<<std::endl;\
		std::cout<<filename.c_str()<<"(1): warning B0001: parameter "<<name<<" was not found."<<filename.c_str()<<std::endl;\
	}

void PlatformConstantBuffer::RestoreDeviceObjects(void *dev,size_t sz,void *addr)
{
	InvalidateDeviceObjects();
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	size=sz;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int bi)
{
	bindingIndex=bi;
	for(int i=0;i<100;i++)
	{
		crossplatform::EffectTechnique *tech=effect->GetTechniqueByIndex(i);
		if(!tech)
			break;
		GLuint program=tech->asGLuint();
		GLint indexInShader;
		indexInShader=glGetUniformBlockIndex(program,name);
		if(indexInShader>=0)
		{
GL_ERROR_CHECK
			glUniformBlockBinding(program,indexInShader,bindingIndex);
					glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
			glBindBufferRange(GL_UNIFORM_BUFFER,bindingIndex,ubo,0,size);	
GL_ERROR_CHECK
		}
		else
			std::cerr<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in the program."<<std::endl;
	}
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr)
{
GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER,ubo);
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
	glBindBuffer(GL_UNIFORM_BUFFER,0);
	glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
GL_ERROR_CHECK
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &deviceContext)
{
}

Effect::Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
	:current_texture_number(0)
{
	filename=filename_utf8;
	platform_effect		=(void*)opengl::CreateEffect(filename_utf8,defines);
}

Effect::~Effect()
{
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	if(asGLint()==-1)
		return NULL;
	GLint e									=asGLint();
	GLuint t								=glfxCompileProgram(e,name);
	if(!t)
	{
		opengl::printEffectLog(e);
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new crossplatform::EffectTechnique;
	tech->platform_technique				=(void*)t;
	techniques[name]						=tech;
	// Now it needs to be in the techniques_by_index list.
	size_t index							=glfxGetProgramIndex(e,name);
	techniques_by_index[index]				=tech;
	return tech;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	if(asGLint()==-1)
		return NULL;
	GLint e									=asGLint();
	int nump								=glfxGetProgramCount(e);
	if(index>=nump)
		return NULL;
	const char *name						=glfxGetProgramName(e,index);
	GLuint t								=glfxCompileProgram(e,name);
	crossplatform::EffectTechnique *tech	=new crossplatform::EffectTechnique;
	techniques[name]						=tech;
	techniques_by_index[index]				=tech;
	tech->platform_technique				=(void*)t;
	return tech;
}

void Effect::SetTexture(const char *name,crossplatform::Texture *tex)
{
	current_texture_number++;
    glActiveTexture(GL_TEXTURE0+current_texture_number);
	glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
GL_ERROR_CHECK
	if(!currentTechnique)
		return;
	GLuint program	=currentTechnique->asGLuint();
	GLint loc		=glGetUniformLocation(program,name);
GL_ERROR_CHECK
	if(loc<0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: texture "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
		glUniform1i(loc,current_texture_number);
GL_ERROR_CHECK
}

void Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
	SetTexture(name,&t);
}

void Effect::SetParameter	(const char *name	,float value)
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform1f(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec2 value)
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform2fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec3 value)	
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform3fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec4 value)	
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,int value)	
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform1i(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetVector		(const char *name	,const float *value)	
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetMatrix		(const char *name	,const float *m)	
{
	GLint loc=glGetUniformLocation(currentTechnique->asGLuint(),name);
	CHECK_PARAM_EXISTS
	SIMUL_ASSERT_WARN(loc>=0,(std::string("Parameter not found in GL Effect: ")+name).c_str());
	glUniformMatrix4fv(loc,1,false,m);
	GL_ERROR_CHECK
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	GL_ERROR_CHECK
	glUseProgram(effectTechnique->asGLuint());
	GL_ERROR_CHECK
	current_texture_number	=0;
	currentTechnique		=effectTechnique;
	apply_count++;
}
void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	glUseProgram(0);
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	currentTechnique=NULL;
	apply_count--;
}