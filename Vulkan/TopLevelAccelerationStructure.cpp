#include "Platform/Vulkan/TopLevelAccelerationStructure.h"
#include "Platform/Vulkan/BottomLevelAccelerationStructure.h"
#include "Platform/Vulkan/Buffer.h"
#include "Platform/Vulkan/RenderPlatform.h"

using namespace platform;
using namespace vulkan;

/////////////////////////////////
//TopLevelAccelerationStructure//
/////////////////////////////////

TopLevelAccelerationStructure::TopLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:crossplatform::TopLevelAccelerationStructure(r)
{

}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void TopLevelAccelerationStructure::RestoreDeviceObjects()
{
}

void TopLevelAccelerationStructure::InvalidateDeviceObjects()
{
	initialized = false;

	RenderPlatform* vulkanRenderPlatform = reinterpret_cast<RenderPlatform*>(renderPlatform);

	DispatchLoaderDynamic d;
	d.vkDestroyAccelerationStructureKHR = platform::vulkan::vkDestroyAccelerationStructureKHR;
	vulkanRenderPlatform->AsVulkanDevice()->destroyAccelerationStructureKHR(accelerationStructure, nullptr, d);

	vulkanRenderPlatform->PushToReleaseManager(accelerationStructureBuffer, &accelerationStructureBufferAllocation);
	vulkanRenderPlatform->PushToReleaseManager(scratchBuffer, &scratchBufferAllocation);
	vulkanRenderPlatform->PushToReleaseManager(instanceDescsBuffer, &instanceDescsBufferAllocation);
}

vk::AccelerationStructureKHR TopLevelAccelerationStructure::AsVulkanShaderResource(crossplatform::DeviceContext& deviceContext)
{
	if (!initialized)
		BuildAccelerationStructureAtRuntime(deviceContext);
	return accelerationStructure;
}

void TopLevelAccelerationStructure::BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext)
{
	InvalidateDeviceObjects();

#if PLATFORM_SUPPORT_VULKAN_RAYTRACING
	RenderPlatform* vulkanRenderPlatform = reinterpret_cast<RenderPlatform*>(renderPlatform);
	vk::Device* device = vulkanRenderPlatform->AsVulkanDevice();
	if (!vulkanRenderPlatform->CheckDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
	{
		// Initialized, but non-functional.
		initialized = true;
		return;
	}

	if (_instanceDescs.empty())
		return;

	m_InstanceDescs.clear();

	DispatchLoaderDynamic d;
	d.vkGetAccelerationStructureDeviceAddressKHR = platform::vulkan::vkGetAccelerationStructureDeviceAddressKHR;
	d.vkGetAccelerationStructureBuildSizesKHR = platform::vulkan::vkGetAccelerationStructureBuildSizesKHR;
	d.vkCreateAccelerationStructureKHR = platform::vulkan::vkCreateAccelerationStructureKHR;
	d.vkCmdBuildAccelerationStructuresKHR = platform::vulkan::vkCmdBuildAccelerationStructuresKHR;
	
	for (const auto& _instanceDesc : _instanceDescs)
	{
		const auto& BLAS = _instanceDesc.blas;
		const auto& Transform = _instanceDesc.transform;
		BottomLevelAccelerationStructure* vkBLAS = (vulkan::BottomLevelAccelerationStructure*)BLAS;

		vk::AccelerationStructureInstanceKHR instanceDesc;
		instanceDesc.transform.matrix[0][0] = Transform.m00;
		instanceDesc.transform.matrix[0][1] = Transform.m10;
		instanceDesc.transform.matrix[0][2] = Transform.m20;
		instanceDesc.transform.matrix[0][3] = Transform.m30;
		instanceDesc.transform.matrix[1][0] = Transform.m01;
		instanceDesc.transform.matrix[1][1] = Transform.m11;
		instanceDesc.transform.matrix[1][2] = Transform.m21;
		instanceDesc.transform.matrix[1][3] = Transform.m31;
		instanceDesc.transform.matrix[2][0] = Transform.m02;
		instanceDesc.transform.matrix[2][1] = Transform.m12;
		instanceDesc.transform.matrix[2][2] = Transform.m22;
		instanceDesc.transform.matrix[2][3] = Transform.m32;
		instanceDesc.instanceCustomIndex = static_cast<uint32_t>(m_InstanceDescs.size()); // Use current size of the array as custom ID, starting at 0... 
		instanceDesc.mask = 0xFF;
		instanceDesc.instanceShaderBindingTableRecordOffset = _instanceDesc.contributionToHitGroupIdx;
		instanceDesc.flags = VkGeometryInstanceFlagBitsKHR(0);
		instanceDesc.accelerationStructureReference = device->getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(vkBLAS->AsVulkanShaderResource(deviceContext)), d);
		m_InstanceDescs.push_back(instanceDesc);
	}

	vulkanRenderPlatform->CreateVulkanBuffer(nullptr, m_InstanceDescs.size() * sizeof(vk::AccelerationStructureInstanceKHR), 
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
			instanceDescsBuffer, instanceDescsBufferAllocation, "AccelerationStructure Instances");
	void* mappedData = nullptr;
	SIMUL_VK_CHECK((vk::Result)vmaMapMemory(instanceDescsBufferAllocation.allocator, instanceDescsBufferAllocation.allocation, &mappedData));
	if (mappedData)
	{
		memcpy(mappedData, m_InstanceDescs.data(), m_InstanceDescs.size());
		vmaUnmapMemory(instanceDescsBufferAllocation.allocator, instanceDescsBufferAllocation.allocation);
	}

	vk::AccelerationStructureGeometryKHR geometryDesc;
	geometryDesc.flags = vk::GeometryFlagBitsKHR(0);
	geometryDesc.geometryType = vk::GeometryTypeKHR::eInstances;
	geometryDesc.geometry.instances = vk::AccelerationStructureGeometryInstancesDataKHR();
	geometryDesc.geometry.instances.arrayOfPointers = false;
	geometryDesc.geometry.instances.data = device->getBufferAddress(instanceDescsBuffer);
	m_Geometries.push_back(geometryDesc);
	m_PrimitiveCounts.push_back(static_cast<uint32_t>(m_InstanceDescs.size()));
	m_ASBRIs.push_back(vk::AccelerationStructureBuildRangeInfoKHR(static_cast<uint32_t>(m_InstanceDescs.size()), 0, 0, 0));

	m_ASBGI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	m_ASBGI.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	m_ASBGI.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	m_ASBGI.srcAccelerationStructure = VK_NULL_HANDLE;
	m_ASBGI.dstAccelerationStructure = VK_NULL_HANDLE;
	m_ASBGI.geometryCount = static_cast<uint32_t>(m_Geometries.size());
	m_ASBGI.pGeometries = m_Geometries.data();
	m_ASBGI.ppGeometries = nullptr;
	m_ASBGI.scratchData = VK_NULL_HANDLE;

	m_ASBSI.accelerationStructureSize = 0;
	m_ASBSI.updateScratchSize = 0;
	m_ASBSI.buildScratchSize = 0;

	device->getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &m_ASBGI, m_PrimitiveCounts.data(), &m_ASBSI, d);

	auto BuildBuffer = [&](size_t size, vk::BufferUsageFlagBits flags, vk::Buffer& buffer, AllocationInfo& allocationInfo, const char* name) -> void
	{
		vulkanRenderPlatform->CreateVulkanBuffer(nullptr, size,
												 flags | vk::BufferUsageFlagBits::eShaderDeviceAddress,
												 vk::MemoryPropertyFlagBits::eDeviceLocal,
												 buffer, allocationInfo, name);
	};

	BuildBuffer(m_ASBSI.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, accelerationStructureBuffer, accelerationStructureBufferAllocation, "TLAS_AccelerationStructureBuffer");
	BuildBuffer(m_ASBSI.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer, scratchBuffer, scratchBufferAllocation, "TLAS_ScratchBuffer");

	vk::AccelerationStructureCreateInfoKHR asCI;
	asCI.createFlags;
	asCI.buffer = accelerationStructureBuffer;
	asCI.offset = 0;
	asCI.size = m_ASBSI.accelerationStructureSize;
	asCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	asCI.deviceAddress = 0;
	SIMUL_VK_CHECK(device->createAccelerationStructureKHR(&asCI, nullptr, &accelerationStructure, d));

	m_ASBGI.dstAccelerationStructure = accelerationStructure;
	m_ASBGI.scratchData = device->getBufferAddress(vk::BufferDeviceAddressInfo(scratchBuffer));

	m_ASBRIPtrs.reserve(m_ASBRIs.size());
	for (auto& asbri : m_ASBRIs)
	{
		m_ASBRIPtrs.push_back(&asbri);
	}

	deviceContext.asVulkanContext()->buildAccelerationStructuresKHR(1, &m_ASBGI, m_ASBRIPtrs.data(), d);
#endif
	initialized = true;
}
