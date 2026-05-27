#pragma once
#include "Platform/CrossPlatform/ShaderBindingTable.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Vulkan/Allocation.h"

namespace vk
{
	class Buffer;
}

namespace platform
{
	namespace vulkan
	{
		class ShaderBindingTable final : public crossplatform::ShaderBindingTable
		{
		public:
			std::map<crossplatform::ShaderRecord::Type, vk::Buffer> SBTResources;
			std::map<crossplatform::ShaderRecord::Type, vulkan::AllocationInfo> SBTResourceAllocations;
			vk::Device* device = nullptr;

		public:
			ShaderBindingTable();
			~ShaderBindingTable();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r) override;
			void InvalidateDeviceObjects() override;

			std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> GetShaderHandlesFromEffectPass(crossplatform::RenderPlatform* renderPlatform, crossplatform::EffectPass* pass) override;
			void BuildShaderBindingTableResources(crossplatform::RenderPlatform* renderPlatform) override;

			inline const std::map<crossplatform::ShaderRecord::Type, size_t>& GetShaderBindingTableStrides() const { return shaderBindingTableStrides; }
			inline std::map<crossplatform::ShaderRecord::Type, size_t>& GetShaderBindingTableStrides() { return shaderBindingTableStrides; }

			private:
			std::vector<uint8_t> shaderGroupHandles;
		};
	}
}

