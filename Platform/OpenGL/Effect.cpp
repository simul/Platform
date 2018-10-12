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
#include "Simul/Base/Timer.h"

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
    mTotalSize(0),
    mBinding(-1),
    mGPUIsMapped(false)
{
    for (int i = 0; i < mNumBuffers; i++)
    {
        mGPUBuffer[i]   = 0;
    }
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
    glGenBuffers(mNumBuffers, &mGPUBuffer[0]);
    GLenum flags = GL_MAP_WRITE_BIT;
    if (cpu_read)
    {
        flags |= GL_MAP_READ_BIT;
    }
    for (int i = 0; i < mNumBuffers; i++)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mGPUBuffer[i]);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, mTotalSize, init_data, flags);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
    int idx = deviceContext.frame_number % mNumBuffers;
    mGPUIsMapped = true;
	return glMapNamedBuffer(mGPUBuffer[idx],GL_WRITE_ONLY);
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (deviceContext.frame_number >= mNumBuffers)
    {
        // We want to map from the oldest buffer:
        int idx = (deviceContext.frame_number + 1) % mNumBuffers;
        const GLuint64 maxTimeOut = 100000; // 0.1ms
        //GLenum res  = glClientWaitSync(mFences[idx], GL_SYNC_FLUSH_COMMANDS_BIT, maxTimeOut);
        //if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED)
        {
            mCurReadMap = glMapNamedBuffer(mGPUBuffer[idx], GL_READ_ONLY);
            return mCurReadMap;
        }
        //else
        //{
        //    std::cout << "[WARNING] We timeouted while waiting to it to be ready! \n";
        //}
    }
    return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (mCurReadMap)
    {
    int idx = (deviceContext.frame_number + 1) % mNumBuffers;
        mCurReadMap = nullptr;
        glUnmapNamedBuffer(mGPUBuffer[idx]);
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

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
    int idx = deviceContext.frame_number % mNumBuffers;
    if (mBinding == -1)
    {
        mBinding = shaderResource.slot;
    }
    if (mGPUIsMapped)
    {
        mGPUIsMapped = false;
        glUnmapNamedBuffer(mGPUBuffer[idx]);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, mBinding, mGPUBuffer[idx]);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext,crossplatform::Effect* effect,const crossplatform::ShaderResource &shaderResource)
{
    crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext, effect, shaderResource);
	Apply(deviceContext,effect,shaderResource);
}

void PlatformStructuredBuffer::AddFence(crossplatform::DeviceContext& deviceContext)
{
    // Insert a fence:
    if (cpu_read)
    {
        int idx     = deviceContext.frame_number % mNumBuffers;
        mFences[idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
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
    }
}

Effect::Effect()
{
}

void Effect::Load(crossplatform::RenderPlatform* renderPlatform,const char* filename_utf8,const std::map<std::string,std::string>& defines)
{
   // SIMUL_COUT << "Loading OpenGL effect:" << filename_utf8 << std::endl;
	crossplatform::Effect::Load(renderPlatform, filename_utf8,defines);
}

Effect::~Effect()
{
	platform_effect=0;
}   

EffectTechnique* Effect::CreateTechnique()
{
	return new opengl::EffectTechnique(renderPlatform);
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
	
//	HGLRC hglrc	=wglGetCurrentContext();
//	SIMUL_COUT<<(int)hglrc<<std::endl;
    const GLchar* glData    = (const GLchar*)fileData;
    ShaderId                = glCreateShader(type);
    glShaderSource(ShaderId, 1, &glData, 0);
    glCompileShader(ShaderId);
	src=glData;
	name=filename_utf8;
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
	crossplatform::EffectPass* p    = new opengl::EffectPass(renderPlatform);
	passes_by_name[name]            = passes_by_index[i] = p;
	return p;
}

EffectPass::EffectPass(crossplatform::RenderPlatform *r):
    crossplatform::EffectPass(r)
	,mProgramId(0),
    PassName("passname")
{
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
		mHandlesUBO[i]=nullptr;
		for (int j = 0; j < 32; j++)
		{
			mTexturesUBOMapping[i][j] = -1;
		}
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
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
    {
		if (mHandlesUBO[i])
			delete mHandlesUBO[i];
		for (int j = 0; j < 32; j++)
		{
			mTexturesUBOMapping[i][j] = -1;
		}
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
        GLint isLinked = 0;
        glGetProgramiv(mProgramId, GL_LINK_STATUS, &isLinked);
        if (isLinked == 0)
        {
            GLint maxLength = 0;
            glGetProgramiv(mProgramId, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(mProgramId, maxLength, &maxLength, &infoLog[0]);

            SIMUL_CERR << "Failed to link the program for pass: "<<this->PassName.c_str()<<"\n";
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
	bool any=false;
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
		if(mHandlesUBO[i])
			any=true;
	}
	if(!any)
		return;

    crossplatform::ContextState* cs = deviceContext.renderPlatform->GetContextState(deviceContext);
    auto rPlat = (opengl::RenderPlatform*)deviceContext.renderPlatform;

    /*
        For each applied texture, get the view and apply it to
        the _TextureHandles_ UBO:

        uniform _TextureHandles_
        {
            uint64  cloudVolume[16];
            uint64  nextUpdate[16];
            etc.
        }
                    cloudVolume[0]               --->   texture only, used in texelFetch and textureSize operations
                                                        we just set it for every applied texture
                    cloudVolume[samplerSlot + 1] --->   this is the texture + sampling state from 'samplerSlot'
    */
    for (unsigned int i = 0; i < numResourceSlots; i++)
    {
        // Find the texture in the texture assignment:
        int slot                = resourceSlots[i];
        auto ta                 = cs->textureAssignmentMap[slot];
        opengl::Texture* tex    = (opengl::Texture*)ta.texture;
        
        // ... Or we may be selecting it conditionally
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

        // We first bind the texture handle alone (for fetch and get size operations)
        GLuint tview        = tex->AsOpenGLView(ta.resourceType, ta.index, ta.mip, ta.uav);
        GLuint64 thandle    = glGetTextureHandleARB(tview);
        rPlat->MakeTextureResident(thandle);
		for(int j=0;j<crossplatform::ShaderType::SHADERTYPE_COUNT;j++)
		{
			const int &uboOffset=mTexturesUBOMapping[j][slot];
			if(mHandlesUBO[j]!=nullptr&&uboOffset!=-1)
				mHandlesUBO[j]->Update(thandle, uboOffset);
		}

        // Texture + sampler
        for (int i = 0; i < numSamplerResourcerSlots; i++)
        {
            int sslot = samplerResourceSlots[i];
            if (usesSamplerSlot(sslot))
            {
                if (sslot >= 23)
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
				for(int j=0;j<crossplatform::ShaderType::SHADERTYPE_COUNT;j++)
				{
					const int &uboOffset=mTexturesUBOMapping[j][slot];
					if(mHandlesUBO[j]!=nullptr&&uboOffset!=-1)
						mHandlesUBO[j]->Update(chandle, uboOffset + (sizeof(GLuint64) * (sslot + 1)));
				}
            }
        }
    }
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
		if(mHandlesUBO[i])
			mHandlesUBO[i]->Bind(mProgramId);
	}
}

GLuint EffectPass::GetGLId()
{
    return mProgramId;
}

void EffectPass::MapTexturesToUBO(crossplatform::Effect* curEffect)
{
    char kTexHandleUbo []  = "_TextureHandles_X";
	char ext[]={'v','h','d','g','p','c'};
	// Different buffer object for each shader type.
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
		kTexHandleUbo[16]=ext[i];
		GLuint texHandlesIdx        = glGetProgramResourceIndex(mProgramId,GL_UNIFORM_BLOCK,kTexHandleUbo);
		if (texHandlesIdx != GL_INVALID_INDEX)
		{
			// 1) Query the slot and the number of active members
			GLenum uboProps[2]  = { GL_BUFFER_BINDING,GL_NUM_ACTIVE_VARIABLES };
			GLint uboRes[2]     = { -1,-1 };
			glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, texHandlesIdx, 2, uboProps, 2, nullptr, uboRes);
			GLint numActiveMem  = uboRes[1];
			GLint uboSlot       = uboRes[0];

			// If we have active members...
			if (numActiveMem > 0)
			{
				// Create the UBO
				const int numMemberEles = 24; // TO-DO: we can query this but it shold be the same 
				mHandlesUBO[i] = new TexHandlesUBO;
				int uboIndex=glGetUniformBlockIndex(mProgramId, kTexHandleUbo);
				mHandlesUBO[i]->Init(numActiveMem * numMemberEles, mProgramId, uboIndex, uboSlot);

				// 2) Build a list with each member index
				GLenum uboMemProps = GL_ACTIVE_VARIABLES;
				std::vector<GLint> uboMemIndices(numActiveMem);
				glGetProgramResourceiv(mProgramId, GL_UNIFORM_BLOCK, texHandlesIdx, 1, &uboMemProps, numActiveMem, nullptr, uboMemIndices.data());

				// 3) Query info of each member:
				GLenum memProps[5] = 
				{
					GL_NAME_LENGTH, GL_OFFSET,
					GL_REFERENCED_BY_COMPUTE_SHADER,
					GL_REFERENCED_BY_VERTEX_SHADER,GL_REFERENCED_BY_FRAGMENT_SHADER 
				};
				for (const GLint member : uboMemIndices)
				{
					GLint memRes[5] = { -1,-1,-1,-1,-1 };
					glGetProgramResourceiv(mProgramId, GL_UNIFORM, member, 5, memProps, 5, nullptr, memRes);
					if (memRes[2] == 0 && memRes[3] == 0 && memRes[4] == 0)
					{
						continue;
					}
					GLint memStrSize = memRes[0];
					if(memStrSize==-1)
						continue;
					// This is the name of the member, it will look like textureName[0]:
					std::string memberName;
					memberName.resize(memStrSize);
					glGetProgramResourceName(mProgramId, GL_UNIFORM, member, memStrSize, nullptr, (GLchar*)memberName.data());

					// 4) Fix the name:
					size_t arrPos = memberName.find("[0]");
					if (arrPos == std::string::npos)
					{
						SIMUL_BREAK("This can not happen");
					}
					std::string newName(memberName.begin(), memberName.begin() + arrPos);

					// 5) Now that we now the name used by SFXO, we can cache the offset:
					// Each member will take sizeof(uint64_t) * 24, so offsets should be
					// multiples of that number
					GLint baseOffset                = memRes[1]; 
					int texSlot                     = curEffect->GetSlot(newName.c_str());
					if(texSlot<0)
						continue;
					mTexturesUBOMapping[i][texSlot]    = baseOffset;
                
					if ((baseOffset % (sizeof(GLuint64) * 24)) != 0)
					{
						SIMUL_BREAK("This can not happen");
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

void TexHandlesUBO::Init(size_t count, GLuint program, int index, int slot)
{
    Release();

    // Generate the UBO:
    glGenBuffers(1, &mId);
    glBindBuffer(GL_UNIFORM_BUFFER, mId);
	size=count;
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GLuint64) * count, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Setup the binding (the handles UBO is declared without layout):
    glUniformBlockBinding(program, index, slot);
    mSlot = slot;
}

void TexHandlesUBO::Bind(GLuint program)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, mSlot, mId);
}

void TexHandlesUBO::Update(GLuint64 value, size_t offset)
{
	if(offset/sizeof(GLuint64)>=size)
		SIMUL_BREAK("");
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
