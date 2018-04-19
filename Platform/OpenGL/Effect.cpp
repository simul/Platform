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

#include <GL/glew.h>

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

static const int NUM_STAGING_BUFFERS=3;
PlatformStructuredBuffer::PlatformStructuredBuffer()
				:num_elements(0)
				,element_bytesize(0)
				,ssbo(0)
				,read_data(0)
				,write_data(0)
{
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *,int ct,int unit_size,bool ,bool,void *init_data)
{
}

void *PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &)
{
	return nullptr;
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext &)
{
    return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &)
{
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &)
{
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &,void *data)
{
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &,crossplatform::Effect *effect,const char *name)
{
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
}

Effect::Effect()
{
}

crossplatform::ShaderResource Effect::GetShaderResource(const char* name)
{
    return crossplatform::Effect::GetShaderResource(name);
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

crossplatform::EffectTechnique* Effect::GetTechniqueByName(const char* name)
{
    return groupCharMap[0]->GetTechniqueByName(name);
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
    if (groupCharMap[0]->techniques_by_index.find(index) != groupCharMap[0]->techniques_by_index.end())
    {
        return groupCharMap[0]->techniques_by_index[index];
    }
    return nullptr;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index, int mip)
{
    SetUnorderedAccessView(deviceContext, GetShaderResource(name), tex, index, mip);
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index, int mip)
{
    opengl::Texture* gTex = (opengl::Texture*)tex;
    if (gTex)
    {

#if 0
        int selectedMip = mip == -1 ? 0 : mip;
        int allLayers = index == -1;
        if (index == -1)
        {
            index = 0;
        }
        // NOTE: SFX knows if an image is used in load operations,so we could be more accurate (GL_READ/GL_READ_WRITE)
        glBindImageTexture(name.slot, gTex->GetGLMainView(), selectedMip, allLayers, index, GL_READ_WRITE, RenderPlatform::ToGLFormat(tex->GetFormat()));
#else
        GLuint imageView = gTex->AsOpenGLView(name.shaderResourceType, index, mip, true);
        glBindImageTexture(name.slot, imageView, 0, GL_TRUE, 0, GL_READ_WRITE, RenderPlatform::ToGLFormat(tex->GetFormat()));
#endif
    }
    else
    {
        // Unbind it:
        glBindImageTexture(name.slot, 0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F);
    }
}

void Effect::SetTexture(crossplatform::DeviceContext& deviceContext,crossplatform::ShaderResource& shaderResource,crossplatform::Texture *tex,int index,int mip)
{
    if (!shaderResource.valid)
    {
        return;
    }
    crossplatform::ContextState* cs = renderPlatform->GetContextState(deviceContext);
    unsigned long slot = shaderResource.slot;
    unsigned long dim = shaderResource.dimensions;

    crossplatform::TextureAssignment& ta = cs->textureAssignmentMap[slot];
    ta.resourceType = shaderResource.shaderResourceType;
    ta.texture      = (tex&&tex->IsValid() && shaderResource.valid) ? tex : 0;
    ta.dimensions   = dim;
    ta.uav          = false;
    ta.index        = index;
    ta.mip          = mip;
    ta.name         = shaderResource.name;
    cs->textureAssignmentMapValid = false;

    bool found = false;
    for (auto& cta : cs->appliedTextures)
    {
        if (cta.name == shaderResource.name)
        {
            found = true;
            cta = ta;
            break;
        }
    }
    if (!found)
    {
        cs->appliedTextures.push_back(ta);
    }
}

void Effect::SetTexture(crossplatform::DeviceContext& deviceContext,const char* name,crossplatform::Texture *tex,int index,int mip)
{
    crossplatform::ContextState* cs = renderPlatform->GetContextState(deviceContext);
    int slot    = GetSlot(name);
    int dim     = GetDimensions(name);
    crossplatform::TextureAssignment& ta = cs->textureAssignmentMap[slot];
    ta.resourceType = GetResourceType(name);
    ta.texture      = (tex&&tex->IsValid()) ? tex : 0;
    ta.dimensions   = dim;
    ta.uav          = false;
    ta.index        = index;
    ta.mip          = mip;
    ta.name         = name;
    cs->textureAssignmentMapValid = false;

    bool found = false;
    for (auto& cta : cs->appliedTextures)
    {
        if (cta.name == name)
        {
            found = true;
            cta = ta;
            break;
        }
    }
    if (!found)
    {
        cs->appliedTextures.push_back(ta);
    }
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceC,crossplatform::ConstantBufferBase *s)	
{
    RenderPlatform *r = (RenderPlatform *)deviceC.renderPlatform;
    s->GetPlatformConstantBuffer()->Apply(deviceC, s->GetSize(), s->GetAddr());

    crossplatform::Effect::SetConstantBuffer(deviceC, s);
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

}

void Shader::load(crossplatform::RenderPlatform* renderPlatform, const char* filename_utf8, crossplatform::ShaderType t)
{
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
}

void EffectPass::Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) 
{
    // Create the program:
    if (mProgramId == 0)
    {
        auto cs     = deviceContext.contextState;
        PassName    = cs.currentTechnique->name;

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
        {
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
                const GLenum activeProps[1] = { GL_NUM_ACTIVE_VARIABLES };
                GLint activeRes;
                glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, i, 1, activeProps, 1, nullptr, &activeRes);
                if (activeRes <= 0)
                {
                    continue;
                }

                // TO-DO: slot?? ¯\_(ツ)_/¯
                int slot = 0;

                // Create the handles UBO:
                mHandlesUBO = new TexHandlesUBO;
                mHandlesUBO->Init(activeRes * 16,mProgramId,slot);

                // Get active member indices:
                const GLenum activeUnifProp[1] = { GL_ACTIVE_VARIABLES };
                std::vector<GLint> memIdxs(activeRes);
                glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, i, 1, activeUnifProp, activeRes, nullptr, &memIdxs[0]);

                // Build a list of active members:
                const GLenum memProps[3] = { GL_NAME_LENGTH,GL_OFFSET,GL_ARRAY_SIZE };
                const GLenum refProps[3] = { GL_REFERENCED_BY_VERTEX_SHADER,GL_REFERENCED_BY_FRAGMENT_SHADER,GL_REFERENCED_BY_COMPUTE_SHADER };
                for (int j = 0; j < activeRes; j++)
                {
                    GLint refRes[3];
                    glGetProgramResourceiv(mProgramId, GL_UNIFORM, memIdxs[j], 3, refProps, 3, nullptr, refRes);
                    if (refRes[0] || refRes[1] || refRes[2])
                    {
                        GLint memRes[3];
                        glGetProgramResourceiv(mProgramId, GL_UNIFORM, memIdxs[j], 3, memProps, 3, nullptr, &memRes[0]);
                        std::string memName;
                        memName.resize(memRes[0]);
                        glGetProgramResourceName(mProgramId, GL_UNIFORM, memIdxs[j], memName.size(), nullptr, &memName[0]);
                        // Get the texture and sampler names:
                        {
                            size_t idPoss = memName.find("[0]");
                            if (idPoss != std::string::npos && memRes[2] == 16)
                            {
                                std::string texName(memName.begin(),memName.begin() + idPoss);
                                // mTextureMap[texName].push_back({ sampName, memRes[1]});
                                mTextureSet[texName] = memRes[1];
                            }
                            // else
                            // {
                            //     // Texture without a sampler, probably used in a fetch/get size operation:
                            //     mTextureMap[memName].push_back({ "",memRes[1] });
                            // }
                        }
                    }
                }

            }
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
    if (!mHandlesUBO)
    {
        return;
    }

    auto cs = deviceContext.contextState;
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

    for (const auto ta : cs.appliedTextures)
    {
        auto texBinding = mTextureSet.find(ta.name);
        if (texBinding != mTextureSet.end())
        {
            opengl::Texture* tex = (opengl::Texture*)ta.texture;
            if (!tex)
            {
                continue; // dummy tex?
            }

            // Texture view
            GLuint tview        = tex->AsOpenGLView(ta.resourceType, ta.index, ta.mip, ta.uav);
            GLuint64 thandle    = glGetTextureHandleARB(tview);
            rPlat->MakeTextureResident(thandle);
            mHandlesUBO->Update(thandle, texBinding->second);

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
                    auto glsampler      = (opengl::SamplerState*)cs.currentEffect->GetSamplers()[sslot];
                    GLuint sview        = glsampler->asGLuint();
                    GLuint64 chandle    = glGetTextureSamplerHandleARB(tview, sview);
                    rPlat->MakeTextureResident(chandle);
                    mHandlesUBO->Update(chandle, texBinding->second + (sizeof(GLuint64) * (sslot + 1)));
                }
            }
        }
    }

    // For each of the applied textures:
    // for (const auto ta : cs.appliedTextures)
    // {
    //     auto texBinding = mTextureMap.find(ta.name);
    //     if (texBinding != mTextureMap.end())
    //     {
    //         // For each sampler state:
    //         for (const auto sampler : texBinding->second)
    //         {
    //             // TO-DO: dummy textures!!!
    //             opengl::Texture* tex = (opengl::Texture*)ta.texture;
    //             if (!tex)
    //             {
    //                 continue;
    //             }
    //             GLuint tview        = tex->AsOpenGLView(ta.resourceType, ta.index, ta.mip, ta.uav);
    //             GLuint64 thandle    = 0;
    // 
    //             // No sampler (fetch/getsize):
    //             if (sampler.Sampler.empty())
    //             {
    //                 thandle  = glGetTextureHandleARB(tview);
    //             }
    //             // With sampler, lets combine it:
    //             else
    //             {
    //                 opengl::SamplerState* ss    = (opengl::SamplerState*)rPlat->GetOrCreateSamplerStateByName(sampler.Sampler.c_str());
    //                 GLuint sid                  = ss->asGLuint();
    //                 thandle                     = glGetTextureSamplerHandleARB(tview, sid);
    //             }
    //             rPlat->MakeTextureResident(thandle);
    //             mHandlesUBO->Update(thandle, sampler.Offset);
    //         }
    //     }
    // }

    mHandlesUBO->Bind(mProgramId);
}

GLuint EffectPass::GetGLId()
{
    return mProgramId;
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
