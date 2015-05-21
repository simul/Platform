#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glfx.h>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/PrintEffectLog.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

using namespace simul;
using namespace opengl;
using namespace std;

#define CHECK_PARAM_EXISTS\
	if(loc<0)\
	{\
		static bool ignore=false;\
		if(!ignore)\
		{\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLFX program "<<filename.c_str()<<std::endl;\
			std::cerr<<filenameInUseUtf8.c_str()<<"(1): warning B0001: parameter "<<name<<" was not found."<<filename.c_str()<<std::endl;\
			std::cerr<<"Further errors of this type will not be shown."<<std::endl;\
			ignore=true;\
		}\
		return;\
	}

/// Here we break if we have a null technique - but only once.
#define CHECK_TECH_EXISTS\
	if(currentTechnique==NULL)\
	{\
		static bool ignore=false;\
		if(!ignore)\
		{\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): error B0001: currentTechnique is NULL in "<<filename.c_str()<<std::endl;\
			BREAK_IF_DEBUGGING\
			std::cerr<<"Further errors of this type will not be shown."<<std::endl;\
			ignore=true;\
		}\
		return;\
	}

GLenum toGlQueryType(crossplatform::QueryType t)
{
	switch(t)
	{
		case crossplatform::QUERY_OCCLUSION:
			return GL_SAMPLES_PASSED;
		case crossplatform::QUERY_TIMESTAMP:
			return GL_TIMESTAMP;
		default:
			return 0;
	};
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *)
{
GL_ERROR_CHECK
	glGenQueries(QueryLatency,glQuery);
GL_ERROR_CHECK
}

void Query::InvalidateDeviceObjects() 
{
GL_ERROR_CHECK
	glDeleteQueries(QueryLatency,glQuery);
GL_ERROR_CHECK
	for(int i=0;i<QueryLatency;i++)
		glQuery[i]=0;
}

void Query::Begin(crossplatform::DeviceContext &)
{
	currFrame = (currFrame + 1) % QueryLatency;  
GL_ERROR_CHECK
	glBeginQuery(toGlQueryType(type),glQuery[currFrame]);
GL_ERROR_CHECK
}

void Query::End(crossplatform::DeviceContext &)
{
GL_ERROR_CHECK
	glEndQuery(toGlQueryType(type));
GL_ERROR_CHECK
}

void Query::GetData(crossplatform::DeviceContext &,void *data,size_t sz)
{
GL_ERROR_CHECK
	GLuint ok=0;
	glGetQueryObjectuiv(glQuery[currFrame],GL_QUERY_RESULT_AVAILABLE,&ok);
GL_ERROR_CHECK
	if(ok==GL_TRUE)
		glGetQueryObjectuiv(glQuery[currFrame],GL_QUERY_RESULT,(GLuint*)data);
GL_ERROR_CHECK
}


void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *,size_t sz,void *addr)
{
	InvalidateDeviceObjects();
	GL_ERROR_CHECK
	glGenBuffers(1, &ubo);
	GL_ERROR_CHECK
	glBindBuffer(GL_constant_buffer, ubo);
	glBufferData(GL_constant_buffer, sz, addr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_constant_buffer, 0);
	GL_ERROR_CHECK
	size=sz;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
	if(errno!=0)
	{
		DebugBreak();
	}
GL_ERROR_CHECK
	static int lastBindingIndex=21;
	if(lastBindingIndex>=85)
		lastBindingIndex=21;
	bindingIndex=lastBindingIndex;
	lastBindingIndex++;
GL_ERROR_CHECK
	bool any=false;
	for(crossplatform::TechniqueMap::iterator i=effect->techniques.begin();i!=effect->techniques.end();i++)
	{
		crossplatform::EffectTechnique *tech=i->second;
		if(!tech)
			break;
		for(int j=0;j<tech->NumPasses();j++)
		{
	GL_ERROR_CHECK
			GLuint program=tech->passAsGLuint(j);
			GLint indexInShader;
	GL_ERROR_CHECK
			indexInShader=glGetUniformBlockIndex(program,name);
			if(indexInShader==GL_INVALID_INDEX)
			{
				ResetGLError();
				continue;
			}
	GL_ERROR_CHECK
			if(indexInShader>=0)
			{
				any=true;
	GL_ERROR_CHECK
				glUniformBlockBinding(program,indexInShader,bindingIndex);
				int err=glGetError();
				if(err)
				{
					// No good reason why this might happen, but it sometimes does - driver-dependent.
					continue;
				}
				glBindBufferBase(GL_constant_buffer,bindingIndex,ubo);
	GL_ERROR_CHECK
				glBindBufferRange(GL_constant_buffer,bindingIndex,ubo,0,size);	
	GL_ERROR_CHECK
			}
			///else
			//	std::cerr<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in pass "<<j<<" of "<<techname<<std::endl;
		}
	GL_ERROR_CHECK
	}
	GL_ERROR_CHECK
	if(!any)
		std::cerr<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in the effect "<<effect->GetName()<<"."<<std::endl;
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &,size_t size,void *addr)
{
GL_ERROR_CHECK
	glBindBuffer(GL_constant_buffer,ubo);
GL_ERROR_CHECK
	glBufferSubData(GL_constant_buffer,0,size,addr);
GL_ERROR_CHECK
	glBindBuffer(GL_constant_buffer,0);
GL_ERROR_CHECK
//	glBindBufferBase(GL_constant_buffer,bindingIndex,ubo);
//GL_ERROR_CHECK
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{
}

static const int NUM_STAGING_BUFFERS=3;
PlatformStructuredBuffer::PlatformStructuredBuffer()
				:num_elements(0)
				,element_bytesize(0)
				,ssbo(0)
				,read_data(0)
				,write_data(0)
			{
			}
void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data)
{
	InvalidateDeviceObjects();
	num_elements=ct;
	element_bytesize=unit_size;
	GL_ERROR_CHECK
	glGenBuffers(1, &ssbo);
	GL_ERROR_CHECK
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	GL_ERROR_CHECK
	glBufferData(GL_SHADER_STORAGE_BUFFER, element_bytesize*num_elements, init_data, GL_DYNAMIC_COPY);
	GL_ERROR_CHECK
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	GL_ERROR_CHECK
}

void *PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	GL_ERROR_CHECK
	write_data =(unsigned char*) glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
	GL_ERROR_CHECK
	return write_data;
}

const void *PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	GL_ERROR_CHECK
	read_data =(unsigned char*) glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	GL_ERROR_CHECK
	return read_data;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &)
{
	if(read_data)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	GL_ERROR_CHECK
	read_data=NULL;
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	/*	for(int i=0;i<NUM_STAGING_BUFFERS-1;i++)
		std::swap(stagingBuffers[(NUM_STAGING_BUFFERS-1-i)],stagingBuffers[(NUM_STAGING_BUFFERS-2-i)]);
	lastContext->CopyResource(stagingBuffers[0],buffer);*/
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &deviceContext,void *data)
{
	GL_ERROR_CHECK
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
	GL_ERROR_CHECK
	memcpy(p, data, element_bytesize*num_elements);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	GL_ERROR_CHECK
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &,crossplatform::Effect *effect,const char *name)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	if(write_data)
	{
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	}
	GL_ERROR_CHECK
	for(crossplatform::TechniqueMap::iterator i=effect->techniques.begin();i!=effect->techniques.end();i++)
	{
		crossplatform::EffectTechnique *tech=i->second;
		if(!tech)
			break;
		for(int j=0;j<tech->NumPasses();j++)
		{
	GL_ERROR_CHECK
			GLuint program=tech->passAsGLuint(j);
			GLuint indexInShader;
	GL_ERROR_CHECK
			indexInShader=glGetProgramResourceIndex(program,GL_SHADER_STORAGE_BLOCK,name);
			if(indexInShader>=0xFFFFFFFF)
				continue;
	GL_ERROR_CHECK
			GLuint ssbo_binding_point_index = 2;
			glShaderStorageBlockBinding(program, indexInShader, ssbo_binding_point_index);
	GL_ERROR_CHECK
		}
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{
}
void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(write_data)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	write_data=NULL;
	SAFE_DELETE_BUFFER(ssbo);
	num_elements=0;
}
int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

GLuint EffectTechnique::passAsGLuint(int p)
{
	PassIndexMap::const_iterator i=passes_by_index.find(p);
	if(i!=passes_by_index.end())
		return (GLuint)((uintptr_t)(i->second));
	return 0;
}

GLuint EffectTechnique::passAsGLuint(const char *name)
{
	if(!name)
		return passAsGLuint((int)0);
	std::string n(name);
	PassMap::const_iterator i=passes_by_name.find(n);
	if(i!=passes_by_name.end())
		return (GLuint)((uintptr_t)(i->second));
	return 0;
}

Effect::Effect()
	:current_prog(0)
{
}
//------------------------------------------------------------------------------
void errorCallbackFunc(const char *errMsg)
{
    std::cerr<<errMsg;
}
#include "Simul/Base/DefaultFileLoader.h"
void Effect::Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	filename=filename_utf8;
	bool retry=true;
	platform_effect = (void*)0xFFFFFFFF;
	while(retry)
	{
		std::string fn_utf8(filename_utf8);
		// PREFER to use the platform shader:
		std::string filename_fx(filename_utf8);
		if(filename_fx.find(".")>=filename_fx.length())
			filename_fx+=".glfx";
		filenameInUseUtf8 = simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_fx.c_str(), renderPlatform->GetShaderPathsUtf8());
		if(filenameInUseUtf8.length()==0)
		{
			std::string filename_sfx(filename_utf8);
			if(filename_sfx.find(".")>=filename_fx.length())
				filename_sfx+=".sfx";
			filenameInUseUtf8=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_sfx.c_str(),renderPlatform->GetShaderPathsUtf8());
			fn_utf8+=".sfx";
		}
		else
			fn_utf8+=".glfx";
		if (!filenameInUseUtf8.length())
		{
			SIMUL_CERR << "Effect::Load - file not found: " << filename.c_str() << std::endl;
			return;
		}
		GLint effect = glfxGenEffect();
		vector<string> p = renderPlatform->GetShaderPathsUtf8();

		const char **paths = new const char *[p.size() + 1];
		for (int i = 0; i < p.size(); i++)
			paths[i] = p[i].c_str();
		paths[p.size()] = NULL;


		const char **macros = new const char *[defines.size() + 1];
		const char **defs = new const char *[defines.size() + 1];
		const char **m = macros, **d = defs;
		for (map<string, string>::const_iterator i = defines.begin(); i != defines.end(); i++)
		{
			*m = i->first.c_str();
			*d = i->second.c_str();
			m++;
			d++;
		}
		*m=*d = NULL;

		if (!glfxParseEffectFromFile(effect,fn_utf8.c_str(),paths,macros,defs))
		{
			std::string log=glfxGetEffectLog(effect);
			std::cerr<<log.c_str()<<std::endl;
			DebugBreak();
			delete paths;
			delete defs;
			if(renderPlatform->GetShaderBuildMode()&crossplatform::TRY_AGAIN_ON_FAIL)
				continue;
			else
				break;
		}
		platform_effect		=(void*)effect;
	// If any technique fails, we don't want to proceed until the problem is fixed.
		if(!FillInTechniques()&&IsDebuggerPresent())
		{
			DebugBreak();
			if(renderPlatform->GetShaderBuildMode()&crossplatform::ShaderBuildMode::TRY_AGAIN_ON_FAIL)
				continue;
			else
				break;
		}
		else
			break;
	}
}

Effect::~Effect()
{
	glfxDeleteEffect(asGLint());
	platform_effect=0;
}

EffectTechnique *Effect::CreateTechnique()
{
	return new opengl::EffectTechnique;
}

void Effect::AddPass(std::string groupname,std::string techname, std::string passname, GLuint t)
{
	crossplatform::EffectTechnique *tech = EnsureTechniqueExists(groupname, techname, passname);
	tech->passes_by_name[passname] = (void*)t;
	int pass_idx = (int)tech->passes_by_index.size();
	tech->passes_by_index[pass_idx] = (void*)t;
	tech->pass_indices[passname] = pass_idx;
}

// convert GL programs into techniques and passes.
bool Effect::FillInTechniques()
{
	GLint e				=asGLint();
	if(e<0)
		return false;
	groups.clear();
	int numg = (int)glfxGetTechniqueGroupCount(e);
	for (int i = 0; i < numg; i++)
	{
		std::string group_name=glfxGetTechniqueGroupName(e,i);
		glfxUseTechniqueGroup(e,i);
		int numt = (int)glfxGetTechniqueCount(e);
		for (int j = 0; j < numt; j++)
		{
			std::string tech_name = glfxGetTechniqueName(e,j);
			int num_passes = (int)glfxGetPassCount(e, tech_name.c_str());
			for (int k = 0; k < num_passes; k++)
			{
				std::string pass_name = glfxGetPassName(e, tech_name.c_str(), k);
				GLuint t = glfxCompilePass(e, tech_name.c_str(), pass_name.c_str());
				if(!t)
				{
					std::cerr<<filenameInUseUtf8.c_str()
								<<": error C7555:  there are errors in pass "<<pass_name.c_str()<<" of technique "
								<<tech_name.c_str()<<std::endl;
					opengl::printEffectLog(asGLint());
					return false;
				}
				AddPass(group_name,tech_name, pass_name, t);
			}
		}
	}
	glfxUseTechniqueGroup(e,0);
	int nump			=glfxGetProgramCount(e);
	if (nump)
	{
		for (int i = 0; i < nump; i++)
		{
			std::string name = glfxGetProgramName(e, i);
			GLuint t = glfxCompileProgram(e,NULL,name.c_str());
			if (!t)
			{
				std::cerr << filenameInUseUtf8.c_str() << ": error C7555:  there are errors in the technique " << name.c_str() << std::endl;
				opengl::printEffectLog(asGLint());
				return false;
			}
			AddPass("",name, "main", t);
		}
	}
	// ZERO is a valid number of shaders to have in an effect:
	return true;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	GL_ERROR_CHECK
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	if(asGLint()==-1)
		return NULL;
	GLint e					=asGLint();
	GLuint t				=glfxCompileProgram(e, NULL,name);
	if(!t)
	{
		opengl::printEffectLog(e);
		//BREAK_IF_DEBUGGING
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new opengl::EffectTechnique;
	tech->platform_technique				=(void*)t;
	techniques[name]						=tech;
	// Now it needs to be in the techniques_by_index list.
	size_t index							=glfxGetProgramIndex(e,name);
	techniques_by_index[(int)index]			=tech;
	GL_ERROR_CHECK
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
	GLint e				=asGLint();
	if(index>=(int)techniques.size())
		return NULL;
	const char *name	=glfxGetProgramName(e,index);
	GLuint t			=glfxCompileProgram(e,NULL,name);
	if(!t)
	{
		opengl::printEffectLog(e);
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new opengl::EffectTechnique;
	techniques[name]						=tech;
	techniques_by_index[index]				=tech;
	tech->platform_technique				=(void*)t;
	return tech;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex,int mip)
{
	SetTex(name,tex,true, mip);
}

void Effect::SetTex(const char *name,crossplatform::Texture *tex,bool write,int mip)
{
	GL_ERROR_CHECK
	int texture_number=glfxGetEffectTextureNumber((GLuint)platform_effect,name);
	glfxSetEffectTexture((int)platform_effect,texture_number,tex->AsGLuint());
	GL_ERROR_CHECK
    glActiveTexture(GL_TEXTURE0+texture_number);
	// Fall out silently if this texture is not set.
	GL_ERROR_CHECK
	if(!tex)
		return;
	if(!tex->AsGLuint())
		return;
	if(tex->GetDimension()==2)
	{
		if(write)
		{
			texture_number=0;
			glBindImageTexture(texture_number,
 				tex->AsGLuint(),
 				0,
 				GL_FALSE,
 				0,
 				GL_READ_WRITE,
				opengl::RenderPlatform::ToGLFormat(tex->GetFormat()));
		}
		//glBindImageTexture(0, volume_tid, 0, /*layered=*/GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
		else
		{
			// 2D but depth>1? That's an ARRAY texture.
			if(tex->GetDepth()>1)
				glBindTexture(GL_TEXTURE_2D_ARRAY, tex->AsGLuint());
			else
				glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
		}
	}
	else if(tex->GetDimension()==3)
	{
		if(write)
		{
			texture_number=0;
			glBindImageTexture(texture_number,
 				tex->AsGLuint(),
 				0,
 				GL_TRUE,
 				0,
 				GL_READ_WRITE,
				opengl::RenderPlatform::ToGLFormat(tex->GetFormat()));
		//GL_RGBA32F);
/*
GL_INVALID_VALUE is generated if unit greater than or equal to the value of GL_MAX_IMAGE_UNITS (0x8F38).
GL_INVALID_VALUE is generated if texture is not the name of an existing texture object.
GL_INVALID_VALUE is generated if level or layer is less than zero.
*/
		}
		else
			glBindTexture(GL_TEXTURE_3D,tex->AsGLuint());
	}
	else
		throw simul::base::RuntimeError("Unknown texture dimension!");
    glActiveTexture(GL_TEXTURE0+texture_number);
	if(currentTechnique)
	{
		GLuint program	=currentTechnique->passAsGLuint(currentPass);
		// If we didn't find this pass already, we've already reported the error. Fail silently this time, therefore.
		if(program==0)
			return;
		GLint loc		=glGetUniformLocation(program,name);
		if(loc<0)
			CHECK_PARAM_EXISTS
		glUniform1i(loc,texture_number);
	}
	else
	{
		for(crossplatform::TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
		{
			for(int j=0;j<i->second->NumPasses();j++)
			{
				GLuint program	=i->second->passAsGLuint(j);
				GLint loc		=glGetUniformLocation(program,name);
				if(loc>=0)
				{
					glUseProgram(program);
					glUniform1i(loc,texture_number);
					glUseProgram(0);
				}
			}
		}
	}
GL_ERROR_CHECK
}
void Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex)
{
	SetTex(name,tex,false,0);
}

void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture &t)
{
	SetTexture(deviceContext,name,&t);
}

void Effect::SetSamplerState(crossplatform::DeviceContext &,const char *name,crossplatform::SamplerState *s)
{
	GLuint sampler_state=0;
	if(s)
		sampler_state=s->asGLuint();
	GL_ERROR_CHECK
	int texture_number=glfxGetEffectTextureNumber((GLuint)platform_effect,name);
	// There are two possibilities - we are either within an "apply" state, or not.
	if(apply_count)
		glBindSampler(texture_number, sampler_state);
	if(s)
		prepared_sampler_states[texture_number]=sampler_state;
	else
	{
		map<GLuint,GLuint>::iterator i=prepared_sampler_states.find(texture_number);
		if(i!=prepared_sampler_states.end())
			prepared_sampler_states.erase(i);
	}
	// If we're in an "apply" state, we can remember to unbind the sampler at the corresponding "unapply"
	// But if we're not, can we be SURE that we'll hit the unapply? We might then have a stray sampler state left behind.
	// So instead of binding the sampler now, we store the information and wait for "apply".
}

void Effect::SetParameter	(const char *name	,float value)
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform1f(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec2 value)
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform2fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec3 value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform3fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec4 value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	if(loc<0)
	{
		CHECK_PARAM_EXISTS
	}
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,int value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform1i(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,int2 value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform2i(loc,value.x,value.y);
	GL_ERROR_CHECK
}

void Effect::SetVector		(const char *name	,const float *value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetMatrix		(const char *name	,const float *m)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
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
	apply_count++;
	currentTechnique		=effectTechnique;
	currentPass				=pass;
//	CHECK_TECH_EXISTS
	if(effectTechnique)
	{
		current_prog	=effectTechnique->passAsGLuint(pass);
		glUseProgram(current_prog);
		GL_ERROR_CHECK
		for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
			glBindSampler(i->first,i->second);
		EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
		if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
			glEffectTechnique->passStates[currentPass]->Apply();
		GLuint prog=effectTechnique->passAsGLuint(pass);
		glfxApplyPassState((GLuint)platform_effect,prog);
		GL_ERROR_CHECK
	}
	deviceContext.activeTechnique=effectTechnique;
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	GL_ERROR_CHECK
	apply_count++;
	currentTechnique		=effectTechnique;
	CHECK_TECH_EXISTS
	if(effectTechnique)
	{
		GLuint prog=effectTechnique->passAsGLuint(pass);
		if(prog==0)
		{
			SIMUL_FILE_CERR(this->filenameInUseUtf8.c_str())<<"Pass \""<<pass<<"\" not found in technique \""<<GetTechniqueName(effectTechnique)<<"\" of Effect "<<this->filename.c_str()<<std::endl;
			currentPass=-1;
			current_prog=0;
			deviceContext.activeTechnique=NULL;
			return;
		}
		for(EffectTechnique::PassIndexMap::iterator i=effectTechnique->passes_by_index.begin();
			i!=effectTechnique->passes_by_index.end();i++)
		{
			if(i->second==(void*)prog)
				currentPass				=i->first;
		}
		glUseProgram(prog);
		glfxApplyPassState((GLuint)platform_effect,prog);
		GL_ERROR_CHECK
		for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
			glBindSampler(i->first,i->second);
		current_prog	=prog;
		EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
		if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
			glEffectTechnique->passStates[currentPass]->Apply();
	}
	deviceContext.activeTechnique=currentTechnique;
}

void Effect::Reapply(crossplatform::DeviceContext &)
{
	if(apply_count!=1)
		SIMUL_BREAK("Effect::Reapply can only be called after Apply and before Unapply!")
	GLuint prog=current_prog;
	glUseProgram(prog);
	glfxApplyPassState((GLuint)platform_effect,prog);
	GL_ERROR_CHECK
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	GL_ERROR_CHECK
	glUseProgram(0);
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	currentTechnique=NULL;
	deviceContext.activeTechnique=currentTechnique;
	apply_count--;
	for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
		glBindSampler(i->first,0);
GL_ERROR_CHECK
}

void Effect::UnbindTextures(crossplatform::DeviceContext &)
{
	if(apply_count!=1)
		SIMUL_BREAK("UnbindTextures can only be called after Apply and before Unapply!")
	// Here we should be clearing out all the textures that were set to use the shader.
}