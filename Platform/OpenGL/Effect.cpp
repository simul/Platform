#include <GL/glew.h>
#include <GL/glfx.h>
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

using namespace simul;
using namespace opengl;

void PlatformConstantBuffer::RestoreDeviceObjects(void *dev,size_t sz,void *addr)
{
	InvalidateDeviceObjects();
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	size=sz;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)
{
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
			glUniformBlockBinding(program,indexInShader,bindingIndex);
			glBindBufferRange(GL_UNIFORM_BUFFER,bindingIndex,ubo,0,size);	
		}
		else
			std::cerr<<"ConstantBuffer<> LinkToProgram did not find the buffer named "<<name<<" in the program."<<std::endl;
	}
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr)
{
	glBindBuffer(GL_UNIFORM_BUFFER,ubo);
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
	glBindBuffer(GL_UNIFORM_BUFFER,0);
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &deviceContext)
{
}

Effect::Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
	:current_texture_number(0)
	,currentTechnique(NULL)
{
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
	crossplatform::EffectTechnique *tech	=new crossplatform::EffectTechnique;
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
	int nump								=glfxGetProgramCount(effect);
	const char *name						=glfxGetProgramName(e,index);
	GLuint t								=glfxCompileProgram(e,name);
	crossplatform::EffectTechnique *tech	=new crossplatform::EffectTechnique;
	techniques[name]						=tech;
	techniques_by_index[index]				=tech;
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
	if(loc<=0)
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: texture "<<name<<" was not found in GLSL program "<<program<<std::endl;
	else
		glUniform1i(loc,current_texture_number);
GL_ERROR_CHECK
}

void Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
	SetTexture(name,&t);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass)
{
	glUseProgram(effectTechnique->asGLuint());
	current_texture_number	=0;
	currentTechnique		=effectTechnique;
}