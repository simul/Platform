#include "Shader.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/FileLoader.h"
#include "Simul/Platform/Vulkan/EffectPass.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"

using namespace simul;
using namespace vulkan;


Shader::Shader()
{
}

Shader::~Shader()
{
    Release();
}

void Shader::load(crossplatform::RenderPlatform* r, const char* filename_utf8, crossplatform::ShaderType t)
{
    Release();
	renderPlatform=r;

    type = t;

    simul::base::FileLoader* fileLoader = simul::base::FileLoader::GetFileLoader();
    std::string shaderSourcePath        = renderPlatform->GetShaderBinaryPath() + std::string("/") + filename_utf8;

    // Load the shader source:
    unsigned int fileSize   = 0;
    void* fileData          = nullptr;
	// load spirv file as binary data.
    fileLoader->AcquireFileContents(fileData,fileSize, shaderSourcePath.c_str(), false);
    if (!fileData)
    {
        SIMUL_CERR << "Failed to load the shader:" << filename_utf8;
		return;
    }
	auto createInfo = vk::ShaderModuleCreateInfo()
		.setCodeSize(fileSize)
		.setPCode(reinterpret_cast<const uint32_t*>(fileData));
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
	vk::Device *device=renderPlatform->AsVulkanDevice();
	if (device->createShaderModule( &createInfo, nullptr,&mShader) != vk::Result::eSuccess)
	{
		SIMUL_BREAK_ONCE("failed to create shader module!");
	}
	name=filename_utf8;
}

void Shader::Release()
{
	if(renderPlatform)
	{
		vk::Device *device=renderPlatform->AsVulkanDevice();
		if(device)
			device->destroyShaderModule(mShader, nullptr);
	}
}