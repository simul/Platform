#pragma once

#include "Platform/CrossPlatform/BottomLevelAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Vulkan/Allocation.h"
#include <vulkan/vulkan.hpp>

namespace vk
{
	class AccelerationStructureKHR;
	class Buffer;
}

namespace platform
{
	namespace vulkan
	{
		class BottomLevelAccelerationStructure final : public crossplatform::BottomLevelAccelerationStructure
		{
		public:
			BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			~BottomLevelAccelerationStructure();
			void RestoreDeviceObjects() override;
			void InvalidateDeviceObjects() override;
			vk::AccelerationStructureKHR AsVulkanShaderResource(crossplatform::DeviceContext& deviceContext);
			void BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext) override;

		protected:
		#if PLATFORM_SUPPORT_VULKAN_RAYTRACING
			std::vector<vk::AccelerationStructureGeometryKHR> m_Geometries;
			std::vector<uint32_t> m_PrimitiveCounts;
			vk::AccelerationStructureBuildGeometryInfoKHR m_ASBGI;
			vk::AccelerationStructureBuildSizesInfoKHR m_ASBSI;
			std::vector<vk::AccelerationStructureBuildRangeInfoKHR> m_ASBRIs;
			std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> m_ASBRIPtrs;

			vk::AccelerationStructureKHR accelerationStructure;
			
			// Backing buffer for the AccelerationStructure
			vk::Buffer accelerationStructureBuffer; 
			AllocationInfo accelerationStructureBufferAllocation;
			
			//Build and Update
			vk::Buffer scratchBuffer;
			AllocationInfo scratchBufferAllocation;
			
			//For Geometry only
			vk::Buffer transformsBuffer;
			AllocationInfo transformsBufferAllocation;
		#endif
		};
	}
}