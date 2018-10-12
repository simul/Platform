#include <stdlib.h>
#include <stdio.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/Vulkan/Effect.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/Framebuffer.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/Timer.h"

using namespace simul;
using namespace vulkan;


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
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex)
{
    mBindingSlot = bindingIndex;
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext& deviceContext,size_t size,void* addr)
{
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
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
	return 0;
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (deviceContext.frame_number >= mNumBuffers)
    {
    }
    return nullptr;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
{
    if (mCurReadMap)
    {
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
    }
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
    }
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext& deviceContext)
{
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
    if (mGPUBuffer[0] != 0)
    {
    }
}

Effect::Effect()
{
}

void Effect::Load(crossplatform::RenderPlatform* renderPlatform,const char* filename_utf8,const std::map<std::string,std::string>& defines)
{
   // SIMUL_COUT << "Loading Vulkan effect:" << filename_utf8 << std::endl;
	crossplatform::Effect::Load(renderPlatform, filename_utf8,defines);
}

Effect::~Effect()
{
	platform_effect=0;
}   

EffectTechnique* Effect::CreateTechnique()
{
	return new vulkan::EffectTechnique(renderPlatform);
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
    vulkan::Texture* gTex = (vulkan::Texture*)tex;
    if (gTex)
    {
    }
    else
    {
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
    switch (t)
    {
    case simul::crossplatform::SHADERTYPE_VERTEX:
        break;
    case simul::crossplatform::SHADERTYPE_HULL:
    case simul::crossplatform::SHADERTYPE_DOMAIN:
    case simul::crossplatform::SHADERTYPE_GEOMETRY:
    case simul::crossplatform::SHADERTYPE_COUNT:
        break;
    case simul::crossplatform::SHADERTYPE_PIXEL:
        break;
    case simul::crossplatform::SHADERTYPE_COMPUTE:
        break;
    default:
        break;
    }
	
	//src=glData;
	name=filename_utf8;
}

void Shader::Release()
{
    if (ShaderId != 0)
    {
        ShaderId = 0;
    }
}

crossplatform::EffectPass* EffectTechnique::AddPass(const char* name, int i)
{
	crossplatform::EffectPass* p    = new vulkan::EffectPass(renderPlatform);
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

        vulkan::Shader* v   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
        vulkan::Shader* f   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
        vulkan::Shader* c   = (vulkan::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];
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
    auto rPlat = (vulkan::RenderPlatform*)deviceContext.renderPlatform;

    for (unsigned int i = 0; i < numResourceSlots; i++)
    {
        // Find the texture in the texture assignment:
        int slot                = resourceSlots[i];
        auto ta                 = cs->textureAssignmentMap[slot];
        vulkan::Texture* tex    = (vulkan::Texture*)ta.texture;
        
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
    }
	for(int i=0;i<crossplatform::ShaderType::SHADERTYPE_COUNT;i++)
	{
//		if(mHandlesUBO[i])
//			mHandlesUBO[i]->Bind(mProgramId);
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
    }
}

TexHandlesUBO::TexHandlesUBO()
  //  mId(0)
{
}


TexHandlesUBO::~TexHandlesUBO()
{
    Release();
}

/*
void TexHandlesUBO::Init(size_t count, GLuint program, int index, int slot)
{
    Release();

	size=count;
    mSlot = slot;
}

void TexHandlesUBO::Bind(GLuint program)
{
}

void TexHandlesUBO::Update(GLuint64 value, size_t offset)
{
	if(offset/sizeof(GLuint64)>=size)
		SIMUL_BREAK("");
}
*/
void TexHandlesUBO::Release()
{
 //   if (mId != 0)
    {
  //      mId = 0;
    }
}
