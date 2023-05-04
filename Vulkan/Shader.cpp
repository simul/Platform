#include "Shader.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Vulkan/EffectPass.h"
#include "Platform/Vulkan/RenderPlatform.h"

using namespace simul;
using namespace vulkan;


Shader::Shader()
{
}

Shader::~Shader()
{
    Release();
}

void Shader::load(crossplatform::RenderPlatform *r, const char *filename_utf8,const void *fileData, size_t fileSize, crossplatform::ShaderType t)
{
    Release();
	renderPlatform=r;

    type = t;
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
	SetVulkanName(renderPlatform,mShader,filename_utf8);
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
	renderPlatform=nullptr;
}