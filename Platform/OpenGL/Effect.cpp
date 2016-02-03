#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glfx.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/PrintEffectLog.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

#pragma comment(lib,"glfx")

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

bool Query::GetData(crossplatform::DeviceContext &,void *data,size_t )
{
GL_ERROR_CHECK
	GLuint ok=0;
	glGetQueryObjectuiv(glQuery[currFrame],GL_QUERY_RESULT_AVAILABLE,&ok);
GL_ERROR_CHECK
	if(ok==GL_TRUE)
		glGetQueryObjectuiv(glQuery[currFrame],GL_QUERY_RESULT,(GLuint*)data);
GL_ERROR_CHECK
	return (ok==GL_TRUE);
}


void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *,size_t sz,void *addr)
{
	InvalidateDeviceObjects();
	GL_ERROR_CHECK
	glGenBuffers(1, &ubo);
	GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	GL_ERROR_CHECK
	size=sz;
	lastBindingIndex++;
	if (lastBindingIndex >= 52)//85)
		lastBindingIndex = 1;// 21;
	bindingIndex = lastBindingIndex;
}

int PlatformConstantBuffer::lastBindingIndex=1;

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	if(bindingIndex==lastBindingIndex)
		lastBindingIndex--;
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
	if(errno!=0)
	{
		DebugBreak();
	}
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
				glUniformBlockBinding(program, indexInShader, bindingIndex); 
				int err=glGetError();
				if(err)
				{
					// No good reason why this might happen, but it sometimes does - driver-dependent.
					continue;
				}
			}
		}
	GL_ERROR_CHECK
	}
	GL_ERROR_CHECK
	if(!any)
		SIMUL_CERR<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in the effect "<<effect->GetName()<<"."<<std::endl;
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &,size_t size,void *addr)
{
GL_ERROR_CHECK
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, ubo);
GL_ERROR_CHECK
	glBindBufferRange(GL_UNIFORM_BUFFER, bindingIndex, ubo, 0, size);
GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER,ubo);
GL_ERROR_CHECK
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER,0);
GL_ERROR_CHECK
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
void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *,int ct,int unit_size,bool ,void *init_data)
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

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &)
{
	/*	for(int i=0;i<NUM_STAGING_BUFFERS-1;i++)
		std::swap(stagingBuffers[(NUM_STAGING_BUFFERS-1-i)],stagingBuffers[(NUM_STAGING_BUFFERS-2-i)]);
	lastContext->CopyResource(stagingBuffers[0],buffer);*/
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &,void *data)
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
		write_data=NULL;
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
		// Because GLSL Shader Storage Blocks are not implicitly arrays like Structured Buffers in HLSL,
		// We modify the GLSL to look like this:
		//layout(std430) buffer lightingQueryInputs_
		//{
		//	vec3 lightingQueryInputs[];
		//};
		// So to get the resource index, we need to append an underscore "_" to the actual name.
			string modified_name(name);
			modified_name+="_";
			indexInShader=glGetProgramResourceIndex(program,GL_SHADER_STORAGE_BLOCK,modified_name.c_str());
			if(indexInShader>=0xFFFFFFFF)
				continue;
	GL_ERROR_CHECK
		// the binding points must be different for the different ssb's used in the program. Handle in glfx!
			GLuint ssbo_binding_point_index = indexInShader;
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER,ssbo_binding_point_index,ssbo);
			glShaderStorageBlockBinding(program, indexInShader, ssbo_binding_point_index);
	GL_ERROR_CHECK
		}
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	// GLSL makes no distinction between settting StructuredBuffers for read and for write.
	Apply(deviceContext,effect,name);
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
GL_ERROR_CHECK
	if(write_data)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
GL_ERROR_CHECK
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
GL_ERROR_CHECK
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
GL_ERROR_CHECK
	}
	write_data=NULL;
	SAFE_DELETE_BUFFER(ssbo);
	num_elements=0;
GL_ERROR_CHECK
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
	GL_ERROR_CHECK
	ERRNO_BREAK
	while(retry)
	{
	GL_ERROR_CHECK
	ERRNO_BREAK
		std::string fn_utf8(filename_utf8);
		// PREFER to use the platform shader:
		std::string filename_fx(filename_utf8);
	ERRNO_BREAK
		if(filename_fx.find(".")>=filename_fx.length())
			filename_fx+=".glfx";
		filenameInUseUtf8 = simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_fx.c_str(), renderPlatform->GetShaderPathsUtf8());
GL_ERROR_CHECK
		if(filenameInUseUtf8.length()==0)
		{
	ERRNO_BREAK
			std::string filename_sfx(filename_utf8);
			if(filename_sfx.find(".")>=filename_fx.length())
				filename_sfx+=".sfx";
			filenameInUseUtf8=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_sfx.c_str(),renderPlatform->GetShaderPathsUtf8());
GL_ERROR_CHECK
			fn_utf8+=".sfx";
		}
		else
			fn_utf8+=".glfx";
		if (!filenameInUseUtf8.length())
		{
			SIMUL_CERR << "Effect::Load - file not found: " << filename.c_str() <<"; did you set the paths for this RenderPlatform?"<< std::endl;
			return;
		}
		glfxSetGlslangValidationEnabled(((opengl::RenderPlatform*)renderPlatform)->GetGlslangValidation());
		glfxSetCacheDirectory(renderPlatform->GetShaderBinaryPath());
		GLint effect = glfxGenEffect();
		vector<string> p = renderPlatform->GetShaderPathsUtf8();

		const char **paths = new const char *[p.size() + 1];
		for (int i = 0; i < p.size(); i++)
			paths[i] = p[i].c_str();
		paths[p.size()] = NULL;
		
	GL_ERROR_CHECK
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
	GL_ERROR_CHECK
		glfxSetCacheDirectory(renderPlatform->GetShaderBinaryPath());
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
	GL_ERROR_CHECK
	// If any technique fails, we don't want to proceed until the problem is fixed.
		if(!FillInTechniques()&&IsDebuggerPresent())
		{
			DebugBreak();
	GL_ERROR_CHECK
			if(renderPlatform->GetShaderBuildMode()&crossplatform::ShaderBuildMode::TRY_AGAIN_ON_FAIL)
				continue;
			else
				break;
		}
		else
			break;
	}
	GL_ERROR_CHECK
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
	GL_ERROR_CHECK
	base::Timer t;
	t.StartTime();
	groups.clear();
	int numg = (int)glfxGetTechniqueGroupCount(e);
	for (int i = 0; i < numg; i++)
	{
		std::string group_name=glfxGetTechniqueGroupName(e,i);
		glfxUseTechniqueGroup(e,i);
		int numt = (int)glfxGetTechniqueCount(e);
		for (int j = 0; j<numt; j++)
		{
			std::string tech_name = glfxGetTechniqueName(e,j);
			int num_passes = (int)glfxGetPassCount(e, tech_name.c_str());
			for (int k = 0; k < num_passes; k++)
			{
				std::string pass_name = glfxGetPassName(e, tech_name.c_str(), k);
				GLuint t = glfxCompilePass(e, tech_name.c_str(), pass_name.c_str());
				if(!t)
				{
					std::cerr<<filenameInUseUtf8.c_str()<<": error C7555:  there are errors in pass "<<pass_name.c_str()<<" of technique "<<tech_name.c_str()<<std::endl;
					opengl::printEffectLog(asGLint());
					return false;
				}
				AddPass(group_name,tech_name, pass_name, t);
			}
		}
	}
	SIMUL_CERR<<"Compile "<<t.UpdateTime()/1000.0f<<" s"<<std::endl;
	glfxUseTechniqueGroup(e,0);
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
	GLuint t				=glfxCompileProgram(e, name,NULL);
	if(!t)
	{
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new opengl::EffectTechnique;
	tech->platform_technique				=(void*)t;
	techniques[name]						=tech;
	// Now it needs to be in the techniques_by_index list.
	size_t index							=glfxGetTechniqueIndex(e,name);
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
	const char *name	=glfxGetTechniqueName(e,index);
	GLuint t = glfxCompileProgram(e, name, NULL);
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
	SetTex(name,tex, mip);
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *tex,int mip)
{
	GL_ERROR_CHECK
	int texture_number=(int)shaderResource.platform_shader_resource;
	if(texture_number>=1000)
	{
		texture_number	-=1000;
	}
	else
	{
		SIMUL_CERR<<"Texture was not declared as writeable in the shader."<<std::endl;
		return;
	}
	if(texture_number>=0)
	{
		if(tex)
		{
			bool layered=tex->IsCubemap()||tex->GetDimension()==3;
			glfxSetEffectTexture((int)platform_effect,texture_number,tex->AsGLuint(mip),tex->GetDimension()
			,tex->GetDepth()
			,opengl::RenderPlatform::ToGLFormat(tex->GetFormat())
			,true
			,0
			,false
			,0
			,tex->IsCubemap());
		}
		else
			glfxSetEffectTexture((int)platform_effect,texture_number,0,0,0,opengl::RenderPlatform::ToGLFormat(crossplatform::UNKNOWN),true,mip,false,0,false);
	}
	GL_ERROR_CHECK
}

void Effect::SetTex(const char *name,crossplatform::Texture *tex,int mip)
{
	GL_ERROR_CHECK
	int texture_number=-1;
	texture_number=glfxGetEffectImageNumber((GLuint)platform_effect,name);
	if(texture_number>=0)
	{
		if(tex)
		{
			bool layered=tex->IsCubemap()||tex->GetDimension()==3;
			glfxSetEffectTexture((int)platform_effect,texture_number
			,tex->AsGLuint(mip)
			,tex->GetDimension()
			,tex->GetDepth()
			,opengl::RenderPlatform::ToGLFormat(tex->GetFormat())
			,true
			,0//layered?0:mip
			,false//layered
			,0//layered?mip:0
			,false);
		}
		else
			glfxSetEffectTexture((int)platform_effect,texture_number
			,0
			,0
			,0
			,opengl::RenderPlatform::ToGLFormat(crossplatform::UNKNOWN)
			,true
			,0
			,false
			,0
			,false);
	}
	else
	{
		if(texture_number<0)
			texture_number=glfxGetEffectTextureNumber((GLuint)platform_effect,name);
		if(texture_number>=0)
		{
			if(tex)
			{
				bool layered=false;//tex->IsCubemap()||tex->GetDimension()==3;
				glfxSetEffectTexture((int)platform_effect,texture_number
					,tex->AsGLuint()
					,tex->GetDimension()
					,tex->GetDepth()
				,opengl::RenderPlatform::ToGLFormat(tex->GetFormat())
				,false
				,mip>0?mip:0
				,false
				,0
				,tex->IsCubemap());
			}
			else
				glfxSetEffectTexture((int)platform_effect,texture_number,0,0,0
					,opengl::RenderPlatform::ToGLFormat(crossplatform::UNKNOWN)
					,false
					,0
					,false
					,0
					,false);
		}
	}
	GL_ERROR_CHECK
}

crossplatform::ShaderResource Effect::GetShaderResource(const char *name)
{
	crossplatform::ShaderResource res;
	res.platform_shader_resource=0;
	int var=glfxGetEffectImageNumber((GLuint)platform_effect,name);
	if(var<0)
		var=glfxGetEffectTextureNumber((GLuint)platform_effect,name);
	else
		var+=1000;
	res.platform_shader_resource=(void*)var;
	return res;
}

void Effect::SetTexture(crossplatform::DeviceContext &,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *tex,int mip)
{
	int texture_number=(int)shaderResource.platform_shader_resource;
	bool write=false;
	if(texture_number>=1000)
	{
		texture_number	-=1000;
		write			=true;
	}
	if(tex)
		glfxSetEffectTexture((int)platform_effect,texture_number,tex->AsGLuint(),tex->GetDimension(),tex->GetDepth()
			,opengl::RenderPlatform::ToGLFormat(tex->GetFormat()),write,mip,false,0,tex->IsCubemap());
}

void Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex,int mip)
{
	SetTex(name,tex,mip);
}

void Effect::SetSamplerState(crossplatform::DeviceContext &,const char *name,crossplatform::SamplerState *s)
{
	GLuint sampler_state=0;
	if(s)
		sampler_state=s->asGLuint();
	GL_ERROR_CHECK
		glfxSetEffectSamplerState((GLuint)platform_effect, name, s->asGLuint());
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::ConstantBufferBase *s)	
{
	if(!s)
		return;
	crossplatform::PlatformConstantBuffer *pcb=s->GetPlatformConstantBuffer();
	if(!pcb)
		return;
	opengl::PlatformConstantBuffer *pcbgl=(opengl::PlatformConstantBuffer *)pcb;
	bool any=false;
	for(crossplatform::TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
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
				glUniformBlockBinding(program, indexInShader, pcbgl->GetBindingIndex()); 
				int err=glGetError();
				if(err)
				{
					// No good reason why this might happen, but it sometimes does - driver-dependent.
					continue;
				}
			}
		}
	GL_ERROR_CHECK
	}
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
		GL_ERROR_CHECK
		glfxApply((GLuint)platform_effect,current_prog);
		GL_ERROR_CHECK
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
			SIMUL_FILE_CERR(filenameInUseUtf8.c_str())<<"Pass \""<<pass<<"\" not found in technique \""<<GetTechniqueName(effectTechnique)<<"\" of Effect "<<this->filename.c_str()<<std::endl;
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
		glfxApply((GLuint)platform_effect,prog);
		glfxApplyPassState((GLuint)platform_effect,prog);
		GL_ERROR_CHECK
		//for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
		//	glBindSampler(i->first,i->second);
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
	glfxReapply((GLuint)platform_effect,prog);
	glfxApplyPassState((GLuint)platform_effect,prog);
	GL_ERROR_CHECK
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	GL_ERROR_CHECK
	glfxUnapply((GLuint)platform_effect);
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	currentTechnique=NULL;
	deviceContext.activeTechnique=currentTechnique;
	apply_count--;
	//for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
	//	glBindSampler(i->first,0);
GL_ERROR_CHECK
}

void Effect::UnbindTextures(crossplatform::DeviceContext &)
{
	if(apply_count!=1)
		SIMUL_BREAK("UnbindTextures can only be called after Apply and before Unapply!")
	// Here we should be clearing out all the textures that were set to use the shader.
}