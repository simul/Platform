#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
//#define SIMUL_USE_NVFX
#ifdef SIMUL_USE_NVFX
#include <FXParser.h>
#ifdef _DEBUG
#pragma comment(lib,"FxLibD")
#pragma comment(lib,"FxLibGLD")
#pragma comment(lib,"FxParserD")
#else
#pragma comment(lib,"FxLib")
#pragma comment(lib,"FxLibGL")
#pragma comment(lib,"FxParser")
#endif
#else
#include <GL/glfx.h>
#endif

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

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
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

void Query::Begin(crossplatform::DeviceContext &deviceContext)
{
GL_ERROR_CHECK
	glBeginQuery(toGlQueryType(type),glQuery[currFrame]);
GL_ERROR_CHECK
}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
GL_ERROR_CHECK
	glEndQuery(glQuery[currFrame]);
GL_ERROR_CHECK
	currFrame = (currFrame + 1) % QueryLatency;  
}

void Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
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

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
GL_ERROR_CHECK
	static int lastBindingIndex=21;
	if(lastBindingIndex>=GL_MAX_UNIFORM_BUFFER_BINDINGS)
		lastBindingIndex=1;
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
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
	glBindBuffer(GL_UNIFORM_BUFFER,0);
	glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
GL_ERROR_CHECK
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
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
    char fullpath[200];
    fp = fopen(filenameUtf8.c_str(),"r");
	ERRNO_CHECK
}

void Effect::Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	filename=filename_utf8;
#ifdef SIMUL_USE_NVFX
	int pos=filename.find_last_of(".glfx");
	if(pos>0)
		filename=filename.substr(0,pos-4)+".nvfx";
#endif
	bool retry=true;
	while(retry)
	{
		filenameInUseUtf8	=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename.c_str(),opengl::GetShaderPathsUtf8());
		if(!filenameInUseUtf8.length())
		{
			std::cerr<<"Effect::Load - file not found: "<<filename.c_str()<<std::endl;
			DebugBreak();
			continue;
		}
#ifdef SIMUL_USE_NVFX
		nvFX::IContainer*   fx_Effect       = NULL;
		nvFX::setErrorCallback(errorCallbackFunc);
		nvFX::setIncludeCallback(includeCallbackFunc);
		fx_Effect = nvFX::IContainer::create(filenameInUseUtf8.c_str());
		std::string effect_str=LoadAndPreprocessShaderSource(filenameInUseUtf8.c_str(),defines);
	//	bool bRes = nvFX::loadEffect(fx_Effect,effect_str.c_str(),filenameInUseUtf8.c_str());
		bool bRes = nvFX::loadEffectFromFile(fx_Effect,filenameInUseUtf8.c_str());
		if(!bRes)
		{
			DebugBreak();
			continue;
		}
		ERRNO_CHECK
		platform_effect=fx_Effect;
#else
		platform_effect		=(void*)opengl::CreateEffect(filename_utf8,defines);
#endif
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
#ifdef SIMUL_USE_NVFX
	nvFX::IContainer*   fx_Effect       = (nvFX::IContainer* )platform_effect; 
    nvFX::IContainer::destroy(fx_Effect);
	fx_Effect=NULL;
#endif
}

// convert GL programs into techniques and passes.
bool Effect::FillInTechniques()
{
	GLint e				=asGLint();
	if(e<0)
		return false;
#ifdef SIMUL_USE_NVFX
	nvFX::ITechnique*   fx_Tech         = NULL;
	int nump=0;
	nvFX::IContainer*   fx_Effect       = (nvFX::IContainer* )platform_effect; 
	for(int t=0; fx_Tech = fx_Effect->findTechnique(t); t++)
	{
		nump++;
	}
#else
	int nump			=glfxGetProgramCount(e);
#endif
	if(!nump)
		return false;
	groups.clear();
	for(int i=0;i<nump;i++)
	{
#ifdef SIMUL_USE_NVFX
		nvFX::ITechnique*   t = fx_Effect->findTechnique(i);
		std::string name	=t->getName();
		if(!t->validate())
		{
			std::cerr<<filenameInUseUtf8.c_str()<<": error C7555:  there are errors in the technique "<<name.c_str()<<std::endl;
			return false;
		}
#else
		std::string name	=glfxGetProgramName(e,i);
		GLuint t			=glfxCompileProgram(e,name.c_str());
		if(!t)
		{
			std::cerr<<filenameInUseUtf8.c_str()<<": error C7555:  there are errors in the technique "<<name.c_str()<<std::endl;
			opengl::printEffectLog(asGLint());
			return false;
		}
#endif
		// Now the name will determine what technique and pass it is.
		std::string groupname;
		std::string techname=name;
		std::string passname="main";
		int dotpos1=(int)techname.find("::");
		if(dotpos1>=0)
		{
			groupname	=name.substr(0,dotpos1);
			techname	=name.substr(dotpos1+2,techname.length()-dotpos1-2);
		}
		int dotpos2=(int)techname.find_last_of(".");
		if(dotpos2>=0)
		{
			passname	=techname.substr(dotpos2+1,techname.length()-dotpos2-1);
			techname	=techname.substr(0,dotpos2);
		}
		crossplatform::EffectTechnique *tech=NULL;
		if(groupname.size()>0)
		{
			if(groups.find(groupname)==groups.end())
			{
				groups[groupname]=new crossplatform::EffectTechniqueGroup;
			}
			crossplatform::EffectTechniqueGroup *group=groups[groupname];
			if(group->techniques.find(techname)!=group->techniques.end())
				tech=group->techniques[techname];
			else
			{
				tech								=new opengl::EffectTechnique; 
				int index							=(int)group->techniques.size();
				group->techniques[techname]			=tech;
				group->techniques_by_index[index]	=tech;
			}
			techname=(groupname+"::")+techname;
		}
		if(techniques.find(techname)!=techniques.end())
		{
			if(!tech)
				tech=techniques[techname];
			else
				techniques[techname]=tech;
		}
		else
		{
			if(!tech)
				tech						=new opengl::EffectTechnique; 
			techniques[techname]		=tech;
			int index					=(int)techniques_by_index.size();
			techniques_by_index[index]	=tech;
		}
		tech->passes_by_name[passname]	=(void*)t;
		int pass_idx					=(int)tech->passes_by_index.size();
		tech->passes_by_index[pass_idx]	=(void*)t;
		tech->pass_indices[passname]	=pass_idx;
	}
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
#ifdef SIMUL_USE_NVFX
	nvFX::IContainer*   fx_Effect	=(nvFX::IContainer*)platform_effect; 
	nvFX::ITechnique*   t			=fx_Effect->findTechnique(name);
#else
	GLuint t						=glfxCompileProgram(e,name);
#endif
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
#ifdef SIMUL_USE_NVFX
	size_t index=0;
	for(int i=0;i<techniques.size();i++)
		if(fx_Effect->findTechnique(i)==t)
			index=i;
#else
	size_t index							=glfxGetProgramIndex(e,name);
#endif
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
#ifdef SIMUL_USE_NVFX
	nvFX::IContainer*   fx_Effect	=(nvFX::IContainer*)platform_effect; 
	nvFX::ITechnique*   t			=fx_Effect->findTechnique(index);
	const char *name	=t->getName();
#else
	const char *name	=glfxGetProgramName(e,index);
	GLuint t			=glfxCompileProgram(e,name);
#endif
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

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex)
{
	SetTexture(deviceContext,name,tex);
}

void Effect::SetTexture(crossplatform::DeviceContext &,const char *name,crossplatform::Texture *tex)
{
	GL_ERROR_CHECK
	int texture_number=current_texture_number;
#if 1
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
#else
	current_texture_number++;
#endif
	GL_ERROR_CHECK
    glActiveTexture(GL_TEXTURE0+texture_number);
	// Fall out silently if this texture is not set.
	if(!tex)
		return;
	if(!tex->AsGLuint())
		return;
	if(tex->GetDimension()==2)
		glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
	else if(tex->GetDimension()==3)
		glBindTexture(GL_TEXTURE_3D,tex->AsGLuint());
	else
		throw simul::base::RuntimeError("Unknown texture dimension!");
    glActiveTexture(GL_TEXTURE0+texture_number);
GL_ERROR_CHECK
	if(currentTechnique)
	{
		GLuint program	=currentTechnique->passAsGLuint(currentPass);
		GLint loc		=glGetUniformLocation(program,name);
GL_ERROR_CHECK
		if(loc<0)
		{
			CHECK_PARAM_EXISTS
		}
		glUniform1i(loc,texture_number);
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

void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture &t)
{
	SetTexture(deviceContext,name,&t);
}
void Effect::SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s)
{
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
	GLuint prog=effectTechnique->passAsGLuint(pass);
	for(EffectTechnique::PassIndexMap::iterator i=effectTechnique->passes_by_index.begin();
		i!=effectTechnique->passes_by_index.end();i++)
	{
		if(i->second==(void*)prog)
			currentPass				=i->first;
	}
	//currentPass=effectTechnique->passes_by_index.find((void*)prog);
	glUseProgram(prog);
	GL_ERROR_CHECK
	//current_texture_number	=0;
	current_prog	=prog;
	EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
	if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
		glEffectTechnique->passStates[currentPass]->Apply();
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
	//current_texture_number	=0;
GL_ERROR_CHECK
}

void Effect::UnbindTextures(crossplatform::DeviceContext &)
{
	if(apply_count!=1)
		SIMUL_BREAK("UnbindTextures can only be called after Apply and before Unapply!")
	// Here we should be clearing out all the textures that were set to use the shader.
}