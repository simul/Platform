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

vk::ShaderStageFlagBits Shader::GetShaderStage()
{
	switch (type)
	{
	case crossplatform::SHADERTYPE_UNKNOWN:
		return vk::ShaderStageFlagBits(0);
	case crossplatform::SHADERTYPE_VERTEX:
		return vk::ShaderStageFlagBits::eVertex;
	case crossplatform::SHADERTYPE_HULL:
		return vk::ShaderStageFlagBits::eTessellationEvaluation;
	case crossplatform::SHADERTYPE_DOMAIN:
		return vk::ShaderStageFlagBits::eTessellationControl;
	case crossplatform::SHADERTYPE_GEOMETRY:
		return vk::ShaderStageFlagBits::eGeometry;
	case crossplatform::SHADERTYPE_PIXEL:
		return vk::ShaderStageFlagBits::eFragment;
	case crossplatform::SHADERTYPE_COMPUTE:
		return vk::ShaderStageFlagBits::eCompute;
	case crossplatform::SHADERTYPE_RAY_GENERATION:
		return vk::ShaderStageFlagBits::eRaygenKHR;
	case crossplatform::SHADERTYPE_MISS:
		return vk::ShaderStageFlagBits::eMissKHR;
	case crossplatform::SHADERTYPE_CALLABLE:
		return vk::ShaderStageFlagBits::eCallableKHR;
	case crossplatform::SHADERTYPE_CLOSEST_HIT:
		return vk::ShaderStageFlagBits::eClosestHitKHR;
	case crossplatform::SHADERTYPE_ANY_HIT:
		return vk::ShaderStageFlagBits::eAnyHitKHR;
	case crossplatform::SHADERTYPE_INTERSECTION:
		return vk::ShaderStageFlagBits::eIntersectionKHR;
	case crossplatform::SHADERTYPE_EXPORT:
		return vk::ShaderStageFlagBits(0);
	case crossplatform::SHADERTYPE_MESH:
		return vk::ShaderStageFlagBits::eMeshEXT;
	case crossplatform::SHADERTYPE_AMPLIFICATION:
		return vk::ShaderStageFlagBits::eTaskEXT;
	default:
		return vk::ShaderStageFlagBits(0);
	}

	return vk::ShaderStageFlagBits(0);
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