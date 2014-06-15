#include <GL/glew.h>
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"

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

Effect::Effect()
{
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
	if(!platform_effect)
		return NULL;
	GLuint e=(GLuint)platform_effect;
	crossplatform::EffectTechnique *tech=new crossplatform::EffectTechnique;
	tech->platform_technique=e->GetTechniqueByName(name);
	techniques[name]=tech;
	return tech;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByIndex(int index)
{
}

void Effect::SetTexture(const char *name,crossplatform::Texture *tex)
{
}

void Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
}
