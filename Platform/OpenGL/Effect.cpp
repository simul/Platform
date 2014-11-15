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
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
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
			return;\
		}\
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
			return;\
		}\
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
GL_ERROR_CHECK
	glBeginQuery(toGlQueryType(type),glQuery[currFrame]);
GL_ERROR_CHECK
}

void Query::End(crossplatform::DeviceContext &)
{
GL_ERROR_CHECK
	glEndQuery(glQuery[currFrame]);
GL_ERROR_CHECK
	currFrame = (currFrame + 1) % QueryLatency;  
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
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	GL_ERROR_CHECK
	size=sz;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
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
//		const char *techname=i->first.c_str();
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
				glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
	GL_ERROR_CHECK
				glBindBufferRange(GL_UNIFORM_BUFFER,bindingIndex,ubo,0,size);	
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
	glBindBuffer(GL_UNIFORM_BUFFER,ubo);
GL_ERROR_CHECK
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER,0);
GL_ERROR_CHECK
//	glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
//GL_ERROR_CHECK
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{
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
	std::string n(name);
	PassMap::const_iterator i=passes_by_name.find(n);
	if(i!=passes_by_name.end())
		return (GLuint)((uintptr_t)(i->second));
	return 0;
}

Effect::Effect()
	:current_prog(0)
	,current_texture_number(0)
{
}
//------------------------------------------------------------------------------
void errorCallbackFunc(const char *errMsg)
{
    std::cerr<<errMsg;
}
#include "Simul/Base/DefaultFileLoader.h"
void includeCallbackFunc(const char *incName, FILE *&fp, const char *&buf)
{
	std::string filenameUtf8	=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(incName,opengl::GetShaderPathsUtf8());
	if(!filenameUtf8.length())
	{
		fp=NULL;
		return;
	}
  //  char fullpath[200];
    fp = fopen(filenameUtf8.c_str(),"r");
	ERRNO_CHECK
}

void Effect::Load(crossplatform::RenderPlatform *,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	filename=filename_utf8;
	bool retry=true;
	platform_effect = (void*)0xFFFFFFFF;
	while (retry)
	{
		filenameInUseUtf8 = simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename.c_str(), opengl::GetShaderPathsUtf8());
		if (!filenameInUseUtf8.length())
		{
			SIMUL_CERR << "Effect::Load - file not found: " << filename.c_str() << std::endl;
			return;
		}
		GLint effect = glfxGenEffect();
		vector<string> p = opengl::GetShaderPathsUtf8();

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

		if (!glfxParseEffectFromFile(effect, filename_utf8, paths, macros,defs))
		{
			std::string log	=glfxGetEffectLog(effect);
			std::cerr<<log<<std::endl;
			delete paths;
			delete defs;
			DebugBreak();
			continue;
		}
		delete paths;
		delete defs;
		platform_effect		=(void*)effect;
	// If any technique fails, we don't want to proceed until the problem is fixed.
		if(!FillInTechniques()&&IsDebuggerPresent())
		{
			DebugBreak();
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

void Effect::AddPass(std::string techname, std::string passname, GLuint t)
{
	// Now the name will determine what technique and pass it is.
	std::string groupname;
	int dotpos1 = (int)techname.find("::");
	if (dotpos1 >= 0)
	{
		groupname = techname.substr(0, dotpos1);
		techname = techname.substr(dotpos1 + 2, techname.length() - dotpos1 - 2);
	}
	int dotpos2 = (int)techname.find_last_of(".");
	if (dotpos2 >= 0)
	{
		passname = techname.substr(dotpos2 + 1, techname.length() - dotpos2 - 1);
		techname = techname.substr(0, dotpos2);
	}
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
	int numt = (int)glfxGetTechniqueCount(e);
	if (numt)
	{ 
		for (int i = 0; i < numt; i++)
		{
			std::string tech_name = glfxGetTechniqueName(e, i);
			int num_passes = (int)glfxGetPassCount(e, tech_name.c_str());
			for (int j = 0; j < num_passes; j++)
			{
				std::string pass_name = glfxGetPassName(e, tech_name.c_str(), j);
				GLuint t = glfxCompilePass(e, tech_name.c_str(), pass_name.c_str());
				if (!t)
				{
					std::cerr << filenameInUseUtf8.c_str()
						<< ": error C7555:  there are errors in pass "<<pass_name.c_str()<<" of technique "
						<< tech_name.c_str() << std::endl;
					opengl::printEffectLog(asGLint());
					return false;
				}
				AddPass(tech_name, pass_name, t);
			}
		}
	}
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
			AddPass(name, "main", t);
		}
	}
	// ZERO is a valid number of shaders to have in an effect:
	if (numt==0&&nump==0)
		return true;
	return true;
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
	GLuint t = glfxCompileProgram(e, NULL,name);
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

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex)
{
	SetTex(name,tex,true);
}

void Effect::SetTex(const char *name,crossplatform::Texture *tex,bool write)
{
	GL_ERROR_CHECK
	int texture_number=current_texture_number;
	std::string n(name);
	if(textureNumberMap.find(n)!=textureNumberMap.end())
	{
		texture_number=textureNumberMap[n];
	}
	else
	{
		textureNumberMap[n]=current_texture_number;
		current_texture_number++;
	}
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
			glBindImageTexture(0,
 				tex->AsGLuint(),
 				0,
 				GL_FALSE,
 				0,
 				GL_READ_WRITE,
				opengl::RenderPlatform::ToGLFormat(tex->GetFormat()));
		//glBindImageTexture(0, volume_tid, 0, /*layered=*/GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
		else
		{
			// 2D but depth>1? That's an ARRAY texture.
			if(tex->GetDepth()>1)
				glBindTexture(GL_TEXTURE_2D_ARRAY, tex->AsGLuint());
			else
				glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
		}
GL_ERROR_CHECK
	}
	else if(tex->GetDimension()==3)
	{
		if(write)
		{
			glBindImageTexture(0,
 				tex->AsGLuint(),
 				0,
 				GL_TRUE,
 				0,
 				GL_READ_WRITE,
				opengl::RenderPlatform::ToGLFormat(tex->GetFormat()));
		//GL_RGBA32F);
		}
		else
			glBindTexture(GL_TEXTURE_3D,tex->AsGLuint());
GL_ERROR_CHECK
	}
	else
		throw simul::base::RuntimeError("Unknown texture dimension!");
    glActiveTexture(GL_TEXTURE0+texture_number);
GL_ERROR_CHECK
	if(currentTechnique)
	{
		GLuint program	=currentTechnique->passAsGLuint(currentPass);
		// If we didn't find this pass already, we've already reported the error. Fail silently this time, therefore.
		if(program==0)
			return;
		GLint loc		=glGetUniformLocation(program,name);
GL_ERROR_CHECK
		if(loc<0)
		{
			CHECK_PARAM_EXISTS
		}
		glUniform1i(loc,texture_number);
GL_ERROR_CHECK
	}
	else
	{
GL_ERROR_CHECK
		for(crossplatform::TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
		{
GL_ERROR_CHECK
			for(int j=0;j<i->second->NumPasses();j++)
			{
				GLuint program	=i->second->passAsGLuint(j);
	GL_ERROR_CHECK
				GLint loc		=glGetUniformLocation(program,name);
	GL_ERROR_CHECK
				if(loc>=0)
				{
					glUseProgram(program);
					glUniform1i(loc,texture_number);
					glUseProgram(0);
				}
GL_ERROR_CHECK
			}
		}
	}
GL_ERROR_CHECK
}
void Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex)
{
	SetTex(name,tex,false);
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
	int texture_number=current_texture_number;
	std::string n(name);
	if(textureNumberMap.find(n)!=textureNumberMap.end())
	{
		texture_number=textureNumberMap[n];
	}
	else
	{
		textureNumberMap[n]=current_texture_number;
		current_texture_number++;
	}
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

void Effect::Apply(crossplatform::DeviceContext &,crossplatform::EffectTechnique *effectTechnique,int pass)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	GL_ERROR_CHECK
	apply_count++;
	currentTechnique		=effectTechnique;
	currentPass				=pass;
	CHECK_TECH_EXISTS
	if(effectTechnique)
	{
		current_prog	=effectTechnique->passAsGLuint(pass);
		glUseProgram(current_prog);
		GL_ERROR_CHECK
		for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
			glBindSampler(i->first,i->second);
		//current_texture_number	=0;
		EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
		if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
			glEffectTechnique->passStates[currentPass]->Apply();
		GL_ERROR_CHECK
	}
}

void Effect::Apply(crossplatform::DeviceContext &,crossplatform::EffectTechnique *effectTechnique,const char *pass)
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
			return;
		}
		for(EffectTechnique::PassIndexMap::iterator i=effectTechnique->passes_by_index.begin();
			i!=effectTechnique->passes_by_index.end();i++)
		{
			if(i->second==(void*)prog)
				currentPass				=i->first;
		}
		glUseProgram(prog);
		GL_ERROR_CHECK
		for(map<GLuint,GLuint>::iterator i=prepared_sampler_states.begin();i!=prepared_sampler_states.end();i++)
			glBindSampler(i->first,i->second);
		//current_texture_number	=0;
		current_prog	=prog;
		EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
		if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
			glEffectTechnique->passStates[currentPass]->Apply();
	}
}

void Effect::Reapply(crossplatform::DeviceContext &)
{
	if(apply_count!=1)
		SIMUL_BREAK("Effect::Reapply can only be called after Apply and before Unapply!")
	GLuint prog=current_prog;
	glUseProgram(prog);
	GL_ERROR_CHECK
}

void Effect::Unapply(crossplatform::DeviceContext &)
{
	GL_ERROR_CHECK
	glUseProgram(0);
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	currentTechnique=NULL;
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