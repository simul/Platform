#include "Shader.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Vulkan/EffectPass.h"
#include "Platform/Vulkan/RenderPlatform.h"

using namespace platform;
using namespace vulkan;

Shader::Shader()
{
}

Shader::~Shader()
{
	Release();
}

bool Shader::load(crossplatform::RenderPlatform* r, const char* filename_utf8, const void* fileData, size_t fileSize, crossplatform::ShaderType t)
{
	Release();
	renderPlatform = r;

	type = t;
	vk::ShaderModuleCreateInfo createInfo = vk::ShaderModuleCreateInfo()
						  .setCodeSize(fileSize)
						  .setPCode(reinterpret_cast<const uint32_t*>(fileData));
	vk::Device* device = ((vulkan::RenderPlatform*)renderPlatform)->AsVulkanDevice();
	if (device->createShaderModule(&createInfo, nullptr, &mShader) != vk::Result::eSuccess)
	{
		SIMUL_CERR << "Vulkan error creating " << filename_utf8 << std::endl;
		SIMUL_BREAK_ONCE("failed to create shader module!");
		return false;
	}
	SetVulkanName(renderPlatform, mShader, filename_utf8);
	name = filename_utf8;
	return true;
}

void Shader::Release()
{
	if (renderPlatform)
	{
		vk::Device* device = ((vulkan::RenderPlatform*)renderPlatform)->AsVulkanDevice();
		if (device)
			device->destroyShaderModule(mShader, nullptr);
	}
	renderPlatform = nullptr;
}