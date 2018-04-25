#include <stdlib.h>
#include <stdio.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Base/StringFunctions.h"

using namespace simul;
using namespace opengl;

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
}

void Query::InvalidateDeviceObjects() 
{
}

void Query::Begin(crossplatform::DeviceContext &)
{
}

void Query::End(crossplatform::DeviceContext &)
{
}

bool Query::GetData(crossplatform::DeviceContext &,void *data,size_t )
{
    return false;
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

    glCreateBuffers(1, &mUBOId);
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
    mGPUBuffer(0),
    mReadBuffer(0),
    mTotalSize(0),
    mBinding(-1),
    mGPUIsMapped(false)
{
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
    InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,int ct,int unit_size,bool cpu_read,bool,void* init_data)
{
    InvalidateDeviceObjects();

    mTotalSize      = ct * unit_size;
    this->cpu_read  = cpu_read;

    // Create the SSBO:
    glGenBuffers(1, &mGPUBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer);
    // GL_DYNAMIC_STORAGE_BIT   -> we may call SetData (glBufferSubData)
    // GL_MAP_WRITE_BIT         -> we may call GetBuffer()
    GLenum flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, flags);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Create the readback buffer:
    if (cpu_read)
    {
        glGenBuffers(1, &mReadBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, mReadBuffer);
        // GL_MAP_READ_BIT  -> we want to map and read data
        glBufferStorage(GL_COPY_WRITE_BUFFER, mTotalSize, init_data, GL_MAP_READ_BIT);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
    mGPUIsMapped = true;
	return glMapNamedBuffer(mGPUBuffer,GL_WRITE_ONLY);
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    return glMapNamedBuffer(mReadBuffer, GL_READ_ONLY);
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    glUnmapNamedBuffer(mReadBuffer);
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    glCopyNamedBufferSubData(mGPUBuffer, mReadBuffer, 0, 0, mTotalSize);
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext,void* data)
{
    if (data)
    {
        void* pDataGPU = glMapNamedBuffer(mGPUBuffer, GL_WRITE_ONLY);
        memcpy(pDataGPU, data, mTotalSize);
        glUnmapNamedBuffer(mGPUBuffer);
    }
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const char* name)
{
    if (mBinding == -1)
    {
        mBinding = effect->GetSlot(name);
    }
    if (mGPUIsMapped)
    {
        mGPUIsMapped = false;
        glUnmapNamedBuffer(mGPUBuffer);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, mBinding, mGPUBuffer);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const char* name)
{
	Apply(deviceContext,effect,name);
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
    if (mGPUBuffer != 0)
    {
        glDeleteBuffers(1, &mGPUBuffer);
        mGPUBuffer = 0;
    }
    if (mReadBuffer != 0)
    {
        glDeleteBuffers(1, &mReadBuffer);
        mReadBuffer = 0;
    }
}

Effect::Effect()
{
}

void Effect::Load(crossplatform::RenderPlatform* renderPlatform,const char* filename_utf8,const std::map<std::string,std::string>& defines)
{
    SIMUL_COUT << "Loading OpenGL effect:" << filename_utf8 << std::endl;
	crossplatform::Effect::Load(renderPlatform, filename_utf8,defines);
}

Effect::~Effect()
{
	platform_effect=0;
}   

EffectTechnique* Effect::CreateTechnique()
{
	return new opengl::EffectTechnique;
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
    return techniques_by_index[index];
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index, int mip)
{
    auto res = GetShaderResource(name);
    SetUnorderedAccessView(deviceContext, res, tex, index, mip);

    // if (name == nullptr)
    // {
    //     SetUnorderedAccessView(deviceContext, nullptr, tex, index, mip);
    // }
    // else
    // {
    //     SetUnorderedAccessView(deviceContext, GetShaderResource(name), tex, index, mip);
    // }
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index, int mip)
{
    opengl::Texture* gTex = (opengl::Texture*)tex;
    if (gTex)
    {
        GLuint imageView = gTex->AsOpenGLView(name.shaderResourceType, index, mip, true);
        glBindImageTexture(name.slot, imageView, 0, GL_TRUE, 0, GL_READ_WRITE, RenderPlatform::ToGLFormat(tex->GetFormat()));
    }
    else
    {
        // Unbind it:
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
    if (apply_count != 1)
    {
        SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n", this->filename.c_str()));
    }
    apply_count--;
    crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
    cs->textureAssignmentMapValid = false;
    cs->rwTextureAssignmentMapValid = false;
    crossplatform::Effect::Apply(deviceContext, currentTechnique, currentPass);
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

void Shader::load(crossplatform::RenderPlatform* renderPlatform, const char* filename_utf8, crossplatform::ShaderType t)
{
    Release();

    type = t;

    simul::base::FileLoader* fileLoader = simul::base::FileLoader::GetFileLoader();
    std::string shaderSourcePath        = renderPlatform->GetShaderBinaryPath() + std::string("/") + filename_utf8;

    // Load the shader source:
    unsigned int fileSize   = 0;
    void* fileData          = nullptr;
    fileLoader->AcquireFileContents(fileData,fileSize, shaderSourcePath.c_str(), true);
    if (!fileData)
    {
        SIMUL_CERR << "Failed to load the shader:" << filename_utf8;
    }

    // Start creating the gl shader:
    GLenum type = GL_NONE;
    switch (t)
    {
    case simul::crossplatform::SHADERTYPE_VERTEX:
        type = GL_VERTEX_SHADER;
        break;
    case simul::crossplatform::SHADERTYPE_HULL:
    case simul::crossplatform::SHADERTYPE_DOMAIN:
    case simul::crossplatform::SHADERTYPE_GEOMETRY:
    case simul::crossplatform::SHADERTYPE_COUNT:
        break;
    case simul::crossplatform::SHADERTYPE_PIXEL:
        type = GL_FRAGMENT_SHADER;
        break;
    case simul::crossplatform::SHADERTYPE_COMPUTE:
        type = GL_COMPUTE_SHADER;
        break;
    default:
        break;
    }
    const GLchar* glData    = (const GLchar*)fileData;
    ShaderId                = glCreateShader(type);
    glShaderSource(ShaderId, 1, &glData, 0);
    glCompileShader(ShaderId);
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
	crossplatform::EffectPass* p    = new opengl::EffectPass;
	passes_by_name[name]            = passes_by_index[i] = p;
	return p;
}

EffectPass::EffectPass():
    mProgramId(0),
    mHandlesUBO(nullptr),
    PassName("passname")
{
    for (int i = 0; i < 32; i++)
    {
        mTexturesUBOMapping[i] = -1;
    }
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
    if (mHandlesUBO)
    {
        delete mHandlesUBO;
    }
    for (int i = 0; i < 32; i++)
    {
        mTexturesUBOMapping[i] = -1;
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
        PassName                            = cs->currentTechnique->name;

        opengl::Shader* v   = (opengl::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
        opengl::Shader* f   = (opengl::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
        opengl::Shader* c   = (opengl::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
        mProgramId          = glCreateProgram();
        glObjectLabel(GL_PROGRAM, mProgramId, -1, PassName.c_str());

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
        glLinkProgram(mProgramId);

        // Check link status:
        // TO-DO: Why the debug manager does not catch this?
        GLint isLinked = 0;
        glGetProgramiv(mProgramId, GL_LINK_STATUS, &isLinked);
        if (isLinked == 0)
        {
            GLint maxLength = 0;
            glGetProgramiv(mProgramId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(mProgramId, maxLength, &maxLength, &infoLog[0]);

            SIMUL_CERR << "Failed to link a program! \n";
            SIMUL_COUT << infoLog.data() << std::endl;
            SIMUL_BREAK("");

            InvalidateDeviceObjects();
            return;
        }

        // We want to find out the _TextureHandles_ UBO, then check all the used textureSamplers:
        if (v && f)
        {
            glDetachShader(mProgramId, v->ShaderId);
            glDetachShader(mProgramId, f->ShaderId);
                }
        else
                {
            glDetachShader(mProgramId, c->ShaderId);
                }
        MapTexturesToUBO(cs->currentEffect);
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
    if (!mHandlesUBO)
    {
        return;
    }

    crossplatform::ContextState* cs = deviceContext.renderPlatform->GetContextState(deviceContext);
    auto rPlat = (opengl::RenderPlatform*)deviceContext.renderPlatform;

    /*
        uniform _TextureHandles
        {
            uint64_t texa[16];
            uint64_t texb[16];
                .   
                .
                .
            uint64_t texn[16];
        }
    */

    for (unsigned int i = 0; i < numResourceSlots; i++)
    {
        // Find the texture in the texture assignment:
        int slot                = resourceSlots[i];
        auto ta                 = cs->textureAssignmentMap[slot];
            opengl::Texture* tex = (opengl::Texture*)ta.texture;
            if (!tex)
            {
            if (ta.dimensions == 3)
            {
                tex = rPlat->GetDummy3D();
            }
            else
            {
                tex = rPlat->GetDummy2D();
            }
        }

        int uboOffset = mTexturesUBOMapping[slot];

            // Texture view
        if (uboOffset == -1)
        {
            continue;
        }

        // We first bind the texture handle alone (for fetch and get size operations)
            GLuint tview        = tex->AsOpenGLView(ta.resourceType, ta.index, ta.mip, ta.uav);
            GLuint64 thandle    = glGetTextureHandleARB(tview);
            rPlat->MakeTextureResident(thandle);
        mHandlesUBO->Update(thandle, uboOffset);

            // Texture + sampler
            for (int i = 0; i < numSamplerResourcerSlots; i++)
            {
                int sslot = samplerResourceSlots[i];
                if (usesSamplerSlot(sslot))
                {
                    if (sslot >= 15)
                    {
                        SIMUL_BREAK("");
                    }
                auto effectSamp                     = (opengl::SamplerState*)cs->currentEffect->GetSamplers()[sslot];
                opengl::SamplerState* samplerState  = effectSamp;
                // Check for sampler overrides:
                if (cs->samplerStateOverrides.size() > 0 && cs->samplerStateOverrides.HasValue(sslot))
                {
                    samplerState = (opengl::SamplerState*)cs->samplerStateOverrides[sslot];
                    // If invalid, provide an effect sampler state:
                    if (!samplerState)
                    {
                        samplerState = effectSamp;
                    }
                }
                GLuint sview        = samplerState->asGLuint();
                    GLuint64 chandle    = glGetTextureSamplerHandleARB(tview, sview);
                    rPlat->MakeTextureResident(chandle);
                mHandlesUBO->Update(chandle, uboOffset + (sizeof(GLuint64) * (sslot + 1)));
                }
            }
        }
    mHandlesUBO->Bind(mProgramId);
}

GLuint EffectPass::GetGLId()
{
    return mProgramId;
}

void EffectPass::MapTexturesToUBO(crossplatform::Effect* curEffect)
{
    mUsedTextures.clear();
    GLint numUbos = 0;
    glGetProgramInterfaceiv(mProgramId, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numUbos);
    for (int i = 0; i < numUbos; i++)
    {
        // Get ubo name lenght:
        const GLenum nameProps[1] = { GL_NAME_LENGTH };
        GLint nameRes[1];
        glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, i, 1, nameProps, 1, nullptr, nameRes);

        // Get ubo name:
        std::string name;
        name.resize(nameRes[0]);
        glGetProgramResourceName(mProgramId, GL_UNIFORM_BLOCK, i, name.size(), nullptr, &name[0]);

        // We only care about the tex _TextureHandles_ UBO:
        const char* kTexHandleUbo = "_TextureHandles_";
        if (strcmp(name.c_str(), kTexHandleUbo) != 0)
        {
            continue;
        }

        // Get UBO acvtive members:
        const GLenum uboProps[2] = { GL_NUM_ACTIVE_VARIABLES, GL_BUFFER_BINDING };
        // [0] = Active Members
        // [1] = Binding
        GLint uboRes[2];
        glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, i, 2, uboProps, 2, nullptr, uboRes);
        if (uboRes[0] <= 0)
        {
            continue;
        }

        // Create the handles UBO:
        mHandlesUBO = new TexHandlesUBO;
        mHandlesUBO->Init(uboRes[0] * 16, mProgramId, uboRes[1]);

        // Get active member indices:
        const GLenum activeUnifProp[1] = { GL_ACTIVE_VARIABLES };
        std::vector<GLint> memIdxs(uboRes[0]);
        glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, i, 1, activeUnifProp, uboRes[0], nullptr, memIdxs.data());

        // Build a list of active members:
        const GLenum memProps[3] = { GL_NAME_LENGTH,GL_OFFSET,GL_ARRAY_SIZE };
        const GLenum refProps[3] = { GL_REFERENCED_BY_VERTEX_SHADER,GL_REFERENCED_BY_FRAGMENT_SHADER,GL_REFERENCED_BY_COMPUTE_SHADER };
        for (int j = 0; j < uboRes[0]; j++)
        {
            GLint refRes[3];
            glGetProgramResourceiv(mProgramId, GL_UNIFORM, memIdxs[j], 3, refProps, 3, nullptr, refRes);
            if (refRes[0] || refRes[1] || refRes[2])
            {
                // [0] = Name Lenght
                // [1] = Offset
                // [2] = Array Size
                GLint memRes[3];
                glGetProgramResourceiv(mProgramId, GL_UNIFORM, memIdxs[j], 3, memProps, 3, nullptr, memRes);
                std::vector<GLchar> memNameVec(memRes[0]);
                glGetProgramResourceName(mProgramId, GL_UNIFORM, memIdxs[j], memNameVec.size(), nullptr, memNameVec.data());
                std::string memName = std::string(memNameVec.begin(),memNameVec.end() - 1);
                // Get the texture and sampler names:
                {
                    size_t idPoss = memName.find("[0]");
                    if (idPoss != std::string::npos && memRes[2] == 16)
                    {
                        std::string texName(memName.begin(), memName.begin() + idPoss);        
                        mUsedTextures.push_back(texName);
                        auto res = curEffect->GetShaderResource(mUsedTextures[mUsedTextures.size()-1].c_str());
                        if (!res.valid)
                        {
                            SIMUL_CERR << "Invalid shader resource, could not make a mapping.\n";
                            continue;
                        }
                        mTexturesUBOMapping[res.slot] = memRes[1];
                    }
                }
            }
        }
    }
}

TexHandlesUBO::TexHandlesUBO():
    mId(0)
{
}


TexHandlesUBO::~TexHandlesUBO()
{
    Release();
}

void TexHandlesUBO::Init(size_t count, GLuint program, int slot)
{
    Release();

    // Generate the UBO:
    glGenBuffers(1, &mId);
    glBindBuffer(GL_UNIFORM_BUFFER, mId);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLuint64) * count, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Setupt the binding (the handles UBO is declared without layout):
    glUniformBlockBinding(program, glGetUniformBlockIndex(program, Name), slot);
    mSlot = slot;
}

void TexHandlesUBO::Bind(GLuint program)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, mSlot, mId);
}

void TexHandlesUBO::Update(GLuint64 value, size_t offset)
{
    glBindBuffer(GL_UNIFORM_BUFFER, mId);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(GLuint64), &value);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void TexHandlesUBO::Release()
{
    if (mId != 0)
    {
        glDeleteBuffers(1, &mId);
        mId = 0;
    }
}
