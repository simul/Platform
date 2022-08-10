#include <stdlib.h>
#include <stdio.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Platform/Core/RuntimeError.h"
#include "Platform/GLES/Effect.h"
#include "Platform/GLES/RenderPlatform.h"
#include "Platform/GLES/FramebufferGL.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/Timer.h"
#define GLFW_INCLUDE_ES31
#include <GLFW/glfw3.h>
#include <GLES2/gl2.h>
#include <GLES3/gl2ext.h>
#include <GLES3/gl32.h>

using namespace simul;
using namespace gles;

Query::Query(crossplatform::QueryType t) :crossplatform::Query(t)
{
    for (int i = 0; i < QueryLatency; i++)
    {
        glQuery[i] = 0;
    }

    glGenQueriesEXT(crossplatform::Query::QueryLatency, glQuery);
}
GLenum toGlQueryType(crossplatform::QueryType t)
{
	switch(t)
	{
		case crossplatform::QUERY_TIMESTAMP_DISJOINT:
		case crossplatform::QUERY_TIMESTAMP:
			return GL_TIME_ELAPSED_EXT;
		case crossplatform::QUERY_UNKNOWN:
        case crossplatform::QUERY_OCCLUSION:
          //  return GL_SAMPLES_PASSED;
		default:
			return 0;
	};
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *)
{
	InvalidateDeviceObjects();
	glGenQueriesEXT(crossplatform::Query::QueryLatency, glQuery);
	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i] = true;
		doneQuery[i] = false;
	}
}

void Query::InvalidateDeviceObjects() 
{
	glDeleteQueriesEXT(crossplatform::Query::QueryLatency, glQuery);
	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i] = true;
		doneQuery[i] = false;
	}
}

void Query::Begin(crossplatform::DeviceContext &)
{
	//glQueryCounter(glQuery[currFrame], GL_TIMESTAMP);
}

void Query::End(crossplatform::DeviceContext &)
{
	//glQueryCounter(glQuery[currFrame], GL_TIMESTAMP);

	gotResults[currFrame] = false;
	doneQuery[currFrame] = true;
}

bool Query::GetData(crossplatform::DeviceContext&, void* data, size_t sz)
{
	gotResults[currFrame] = true;
	if (!doneQuery[currFrame])
		return false;

	SIMUL_ASSERT(sizeof(sz) >= sizeof(GLuint64));
	//glGetQueryObjectui64v(glQuery[currFrame], GL_QUERY_RESULT, (GLuint64*)data);
	*(GLuint64*)data /= 1000000; //convert ns to ms
	currFrame = (currFrame + 1) % QueryLatency;
	return (data != nullptr);
}

PlatformConstantBuffer::PlatformConstantBuffer():
    mUBOId(0),
    mBindingSlot(-1)
{
}

PlatformConstantBuffer::~PlatformConstantBuffer()
{
    InvalidateDeviceObjects();
}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void *addr)
{
    InvalidateDeviceObjects();

   // was glCreateBuffers(1, &mUBOId);
    glGenBuffers(1, &mUBOId);

    glBindBuffer(GL_UNIFORM_BUFFER, mUBOId);
    glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
    if (mUBOId != 0)
    {
        glDeleteBuffers(1, &mUBOId);
        mUBOId = 0;
    }
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex)
{
    mBindingSlot = bindingIndex;
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext& deviceContext,size_t size,void* addr)
{
	if(mBindingSlot<0)
		return;
    // Update the UBO data:
    glBindBuffer(GL_UNIFORM_BUFFER, mUBOId);
    glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind to the slot:
    glBindBufferBase(GL_UNIFORM_BUFFER, mBindingSlot, mUBOId);
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext& deviceContext)
{
}

PlatformStructuredBuffer::PlatformStructuredBuffer():
    mTotalSize(0),
    mBinding(-1)
{
    for (int i = 0; i < mNumBuffers; i++)
    {
        mGPUBuffer[i]   = 0;
    }
}

bool PlatformStructuredBuffer::IsBufferMapped(size_t idx)
{
    GLint mapped;
    glBindBuffer(GL_UNIFORM_BUFFER, mGPUBuffer[idx]);
    glGetBufferParameteriv(mGPUBuffer[idx], GL_BUFFER_MAPPED, &mapped);
    return mapped == GL_TRUE;
}
PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
    InvalidateDeviceObjects();
}
#ifdef _MSC_VER
#define SIMUL_GL_MAP_PERSISTENT_WRITE_BUFFER 1
#else
#define SIMUL_GL_MAP_PERSISTENT_WRITE_BUFFER 0
#endif
void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,int ct,int unit_size,bool computable,bool cpu_read,void* init_data,const char *n,crossplatform::BufferUsageHint b)
{
    InvalidateDeviceObjects();
	bufferUsageHint = b;
    mTotalSize      = (size_t)ct * (size_t)unit_size;
    this->cpu_read  = cpu_read;
    
#if SIMUL_GL_MAP_PERSISTENT_WRITE_BUFFER
    GLenum flags = GL_MAP_WRITE_BIT;
    if (cpu_read)
    {
        flags |= GL_MAP_READ_BIT;
    }
    if (bufferUsageHint == crossplatform::BufferUsageHint::ONCE)
    {
     //   flags |= GL_MAP_PERSISTENT_BIT; not supported GLES2
    }
    #endif
    
    // Create the SSBO:
    if (bufferUsageHint == crossplatform::BufferUsageHint::ONCE)
    {
        glGenBuffers(1, &mGPUBuffer[0]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[0]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, GL_SHADER_STORAGE_BUFFER);
      //  glBufferStorage(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, flags);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        mGPUBuffer[1] = mGPUBuffer[2] = 0;
    }
    else
    {
        glGenBuffers(mNumBuffers, &mGPUBuffer[0]);
        
        for (int i = 0; i < mNumBuffers; i++)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[i]);
           // glBufferStorage(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, flags);
            glBufferData(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
#if !SIMUL_GL_MAP_PERSISTENT_WRITE_BUFFER
    int idx = GetIndex(deviceContext);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[idx]);
    if (IsBufferMapped(idx))
    {
        GLboolean unmap_success = glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        if (unmap_success != GL_TRUE)
        {
            SIMUL_COUT << "The structured buffer at binding " << mBinding << " did not unmap successfully.";
            SIMUL_BREAK_ONCE("");
        }
    }

    return glMapBufferOES(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
#else
    if (!mMappedWritePtr)
    {
        if (IsBufferMapped(0))
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[0]);
            glUnmapBufferOES(GL_SHADER_STORAGE_BUFFER);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[0]);
        //mMappedWritePtr = glMapNamedBuffer(mGPUBuffer[0], GL_WRITE_ONLY);
        mMappedWritePtr = glMapBufferOES(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    }
    return mMappedWritePtr;
#endif
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (!cpu_read)
        return nullptr;

    if (deviceContext.frame_number >= mNumBuffers && mBinding != -1)
	{
		// We want to map from the oldest buffer:
		int idx = GetIndex(deviceContext, 1);
		/*const GLuint64 maxTimeOut = 100000; // 0.1ms
		if (!glIsSync(mFences[idx]))
		{
			#if _DUBUG
			SIMUL_COUT << "The sync object associated with the structured buffer at binding " << mBinding << " is invalid. Can not map the buffer.\n";
			SIMUL_BREAK_ONCE("");
			#endif

			mFences[idx] = nullptr;
			return nullptr;
		}*/
		
        GLenum res = GL_ALREADY_SIGNALED; // glClientWaitSync(mFences[idx], GL_SYNC_FLUSH_COMMANDS_BIT, maxTimeOut);
		if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED)
		{
            if(!IsBufferMapped(idx))
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[idx]);
                mCurReadMap = glMapBufferOES(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
                return mCurReadMap;
            }
            else
            {
                return mCurReadMap;
            }
		}
		else
		{
			#if _DUBUG
			SIMUL_COUT << "The structured buffer at binding " << mBinding << " is still in use. Can not map the buffer.\n";
			SIMUL_BREAK_ONCE("");
			#endif

			return nullptr;
		}
	}
	return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (!cpu_read)
        return;

    int idx = GetIndex(deviceContext);
    if (IsBufferMapped(idx))
    {
        mCurReadMap = nullptr;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[idx]);
		GLboolean unmap_success = glUnmapBuffer(mGPUBuffer[idx]); 
		if (unmap_success != GL_TRUE)
		{
			#if _DUBUG
			SIMUL_COUT << "The structured buffer at binding " << mBinding << " did not unmap successfully. Buffer assumed to be corrupt.\n";
			SIMUL_BREAK_ONCE("");
			#endif

			glDeleteBuffers(1, &mGPUBuffer[idx]);
			glGenBuffers(1, &mGPUBuffer[idx]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[idx]);
			glBufferData(GL_SHADER_STORAGE_BUFFER, mTotalSize, nullptr, cpu_read ? GL_MAP_WRITE_BIT | GL_MAP_READ_BIT : GL_MAP_WRITE_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
    }
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext,void* data)
{
    if (data)
    {
        //void* pDataGPU = glMapNamedBuffer(mGPUBuffer, GL_WRITE_ONLY);
        //memcpy(pDataGPU, data, mTotalSize);
        //glUnmapNamedBuffer(mGPUBuffer);
    }
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& shaderResource) 
{
    Apply(deviceContext, nullptr, shaderResource);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& shaderResource) 
{
    ApplyAsUnorderedAccessView(deviceContext, nullptr, shaderResource);
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
// mLastIdx is the one we last used to write to.
    int idx = mLastIdx;
    if (mBinding != shaderResource.slot)
    {
        mBinding = shaderResource.slot;
    }
    //Will unmap write only buffers, unless the buffer is persistently mapped.
    //Otherwise: Read only should be unmapped by CloseReadBuffer().
    #if !SIMUL_GL_MAP_PERSISTENT_WRITE_BUFFER
    if (IsBufferMapped(idx)) 
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[idx]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    #endif
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, mBinding, mGPUBuffer[idx]);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
    mLastIdx = GetIndex(deviceContext);
	Apply(deviceContext,effect,shaderResource);
}

void PlatformStructuredBuffer::AddFence(crossplatform::DeviceContext& deviceContext)
{
    // Insert a fence:
    if (cpu_read)
    {
        int idx     = mLastIdx;
		
		//if (glIsSync((Glsync)mFences[idx]))
		{
		//	glDeleteSync((Glsync)mFences[idx]);
			mFences[idx] = nullptr;
		}
		//else
			mFences[idx] = nullptr;

        //mFences[idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
    if (mGPUBuffer[0] != 0)
    {
        glDeleteBuffers(mNumBuffers, &mGPUBuffer[0]);
		for (int i = 0; i < mNumBuffers; i++)
		{
			mGPUBuffer[i] = 0;

			/*if (glIsSync(mFences[i]))
			{
				glDeleteSync(mFences[i]);
				mFences[i] = nullptr;
			}
			else*/
				mFences[i] = nullptr;
		}
    }
}

Effect::Effect()
{
}

bool Effect::Load(crossplatform::RenderPlatform* r, const char* filename_utf8)
{
    if (EnsureEffect(r, filename_utf8))
        return crossplatform::Effect::Load(r, filename_utf8);
    else
        return false;
}

Effect::~Effect()
{
	platform_effect=0;
}   

EffectTechnique* Effect::CreateTechnique()
{
	return new gles::EffectTechnique(renderPlatform,this);
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
    return techniques_by_index[index];
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index, int mip)
{
    auto res = GetShaderResource(name);
    SetUnorderedAccessView(deviceContext, res, tex, index, mip);
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext,const crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index, int mip)
{
    if (!name.valid)
        return;

    gles::Texture* gTex = (gles::Texture*)tex;
	if (gTex)
	{
		GLuint imageView = gTex->AsOpenGLView(name.shaderResourceType, index, mip, true);
		if (glIsTexture(imageView))
		{
			glBindImageTexture(name.slot, imageView, 0, GL_TRUE, 0, GL_READ_WRITE, RenderPlatform::ToGLInternalFormat(tex->GetFormat()));
		}
		else
		{
			// Unbind it:
			glBindImageTexture(name.slot, 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
		}
	}
    else
    {
        // Unbind it:
        if(name.slot>=0&&name.slot<GL_MAX_IMAGE_UNITS)
            glBindImageTexture(name.slot, 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    }
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext& deviceContext,crossplatform::ConstantBufferBase* s)
{
    RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
    s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());

    crossplatform::Effect::SetConstantBuffer(deviceContext, s);
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,int pass)
{
    crossplatform::Effect::Apply(deviceContext, effectTechnique, pass);
    deviceContext.contextState.currentEffectPass->Apply(deviceContext, true);
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,const char* pass)
{
    crossplatform::Effect::Apply(deviceContext, effectTechnique, pass);
    deviceContext.contextState.currentEffectPass->Apply(deviceContext, true);
}

void Effect::Reapply(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
    auto *p=cs->currentEffectPass;
    crossplatform::Effect::Unapply(deviceContext);
    cs->textureAssignmentMapValid = false;
    cs->rwTextureAssignmentMapValid = false;
    crossplatform::Effect::Apply(deviceContext, p);
}

void Effect::Unapply(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::Unapply(deviceContext);
}

void Effect::UnbindTextures(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::UnbindTextures(deviceContext);
}

Shader::Shader():
    ShaderId(0)
{
}

Shader::~Shader()
{
    Release();
}

void Shader::load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *fileData, size_t DataSize, crossplatform::ShaderType t)
{
    Release();

    type = t;
    // Start creating the gl shader:
    GLenum type = GL_NONE;
    switch (t)
    {
    case simul::crossplatform::SHADERTYPE_VERTEX:
        type = GL_VERTEX_SHADER;
        break;
    case simul::crossplatform::SHADERTYPE_PIXEL:
        type = GL_FRAGMENT_SHADER;
        break;
    case simul::crossplatform::SHADERTYPE_COMPUTE:
        type = GL_COMPUTE_SHADER;
        break;
    case simul::crossplatform::SHADERTYPE_COUNT:
    default:
        break;
    }
	
    const GLchar* glData    = (const GLchar*)fileData;
    GLuint shaderId         = glCreateShader(type);
     shaderId         = glCreateShader(type);
	GLint sz				= (GLint)DataSize;
	
	src						= std::string(glData, sz);
	name					= filename_utf8;
    
	glShaderSource(shaderId, 1, &glData, &sz);
    glCompileShader(shaderId);

	// Check compile status:
	GLint isCompiled = 0;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == 0)
	{
		GLint maxLength = 0;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> infoLog(maxLength);
		glGetShaderInfoLog(shaderId, maxLength, &maxLength, infoLog.data());

		SIMUL_CERR << "Failed to compile the shader: " << filename_utf8 << "\n";
        if(infoLog.data() && infoLog.size())
		    SIMUL_COUT << infoLog.data() << std::endl;
		SIMUL_BREAK_ONCE("");
	}
    else
    {
        ShaderId = shaderId;
    }
}

void Shader::Release()
{
    if (ShaderId != 0)
    {
        glDeleteShader(ShaderId);
        ShaderId = 0;
    }
}

crossplatform::EffectPass* EffectTechnique::AddPass(const char* name, int i)
{
	crossplatform::EffectPass* p    = new gles::EffectPass(renderPlatform,effect);
	passes_by_name[name]            = passes_by_index[i] = p;
	return p;
}

EffectPass::EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e):
    crossplatform::EffectPass(r,e)
	,mProgramId(0)
{
		}

EffectPass::~EffectPass()
{
    InvalidateDeviceObjects();
}

void EffectPass::InvalidateDeviceObjects()
{
    if (mProgramId != 0)
    {
        glDeleteProgram(mProgramId);
        mProgramId = 0;
    }
    mUsedTextures.clear();
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) 
{
    // Create the program:
    if (mProgramId == 0)
    {
        InvalidateDeviceObjects();
        crossplatform::ContextState* cs     = deviceContext.renderPlatform->GetContextState(deviceContext);
       // name                            = cs->currentEffectPass->name;

        gles::Shader* v   = (gles::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
        gles::Shader* f   = (gles::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
        gles::Shader* c   = (gles::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
        mProgramId          = glCreateProgram();
        //glObjectLabel(GL_PROGRAM, mProgramId, -1, name.c_str());

        int attachedShaders = 0;
        // GFX:
        if (v && f)
        {
            glAttachShader(mProgramId, v->ShaderId);
            glAttachShader(mProgramId, f->ShaderId);
        }
        // Compute:
        else
        {
            glAttachShader(mProgramId, c->ShaderId);
        }
        glGetProgramiv(mProgramId, GL_ATTACHED_SHADERS, &attachedShaders);

        //Link program:
        if ((v && f && attachedShaders == 2) || (c && attachedShaders == 1))
        {
            glLinkProgram(mProgramId);
            glValidateProgram(mProgramId);
        }
        else
        {
            SIMUL_CERR << "Failed to attach shader to the program for pass: " << this->name.c_str() << "\n";
            SIMUL_BREAK("");
        }
		
        // Check link and validate status:

        GLint isLinked = 0;
        glGetProgramiv(mProgramId, GL_LINK_STATUS, &isLinked);
        GLint isValid = 0;
        glGetProgramiv(mProgramId, GL_VALIDATE_STATUS, &isValid);

        if (isLinked == 0 || isValid == 0)
        {
            GLint maxLength = 0;
            glGetProgramiv(mProgramId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(mProgramId, maxLength, &maxLength, &infoLog[0]);
            
            if (!isLinked)
            {
                SIMUL_CERR << "Failed to link the program for pass: " << this->name.c_str() << "\n";
            }
            if (!isValid)
            {
                SIMUL_CERR << "Failed to validate the program for pass: " << this->name.c_str() << "\n";
            }
            SIMUL_COUT << infoLog.data() << std::endl;
            SIMUL_BREAK_ONCE("");

            InvalidateDeviceObjects();
            return;
        }

        // Perform sfxo slot mapping to the _TextureHandles_ UBO offsets:
        MapTexturesToUBO(cs->currentEffect);

        // Detach the shaders:
        if (v && f)
        {
            glDetachShader(mProgramId, v->ShaderId);
            glDetachShader(mProgramId, f->ShaderId);
        }
        else
        {
            glDetachShader(mProgramId, c->ShaderId);
        }
    }

    // Activate the program!
    GLint curProgram = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &curProgram);
    if (curProgram != mProgramId)
    {
        glUseProgram(mProgramId);
    }

    // If valid, activate render states:
    if (blendState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, blendState);
    }
    if (depthStencilState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, depthStencilState);
    }
    if (rasterizerState)
    {
        deviceContext.renderPlatform->SetRenderState(deviceContext, rasterizerState);
    }
}

void EffectPass::SetTextureHandles(crossplatform::DeviceContext & deviceContext)
{
	}

GLuint EffectPass::GetGLId()
{
    return mProgramId;
}

void EffectPass::MapTexturesToUBO(crossplatform::Effect* curEffect)
{
					}
