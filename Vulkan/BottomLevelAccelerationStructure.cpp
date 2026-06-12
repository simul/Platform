#include "Platform/Vulkan/BottomLevelAccelerationStructure.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Buffer.h"
#include "Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Platform/CrossPlatform/Mesh.h"

using namespace platform;
using namespace vulkan;

////////////////////////////////////
//BottomLevelAccelerationStructure//
////////////////////////////////////

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:crossplatform::BottomLevelAccelerationStructure(r)
{
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void BottomLevelAccelerationStructure::RestoreDeviceObjects()
{
}

void BottomLevelAccelerationStructure::InvalidateDeviceObjects()
{
	initialized = false;

	RenderPlatform* vulkanRenderPlatform = reinterpret_cast<RenderPlatform*>(renderPlatform);

	DispatchLoaderDynamic d;
	d.vkDestroyAccelerationStructureKHR = platform::vulkan::vkDestroyAccelerationStructureKHR;
	vulkanRenderPlatform->AsVulkanDevice()->destroyAccelerationStructureKHR(accelerationStructure, nullptr, d);

	vulkanRenderPlatform->PushToReleaseManager(accelerationStructureBuffer, &accelerationStructureBufferAllocation);
	vulkanRenderPlatform->PushToReleaseManager(scratchBuffer, &scratchBufferAllocation);
	vulkanRenderPlatform->PushToReleaseManager(transformsBuffer, &transformsBufferAllocation);
}

vk::AccelerationStructureKHR BottomLevelAccelerationStructure::AsVulkanShaderResource(crossplatform::DeviceContext& deviceContext)
{
	if (!initialized)
		BuildAccelerationStructureAtRuntime(deviceContext);
	return accelerationStructure;
}

void BottomLevelAccelerationStructure::BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext)
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

	DispatchLoaderDynamic d;
	d.vkGetAccelerationStructureBuildSizesKHR = platform::vulkan::vkGetAccelerationStructureBuildSizesKHR;
	d.vkCreateAccelerationStructureKHR = platform::vulkan::vkCreateAccelerationStructureKHR;
	d.vkCmdBuildAccelerationStructuresKHR = platform::vulkan::vkCmdBuildAccelerationStructuresKHR;

	if(geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::TRIANGLE_MESH)
	{
		crossplatform::Buffer* indexBuffer = mesh->GetIndexBuffer();
		crossplatform::Buffer* vertexBuffer = mesh->GetVertexBuffer();
		if (!indexBuffer || !vertexBuffer)
			return;

		auto indexBufferVK = reinterpret_cast<Buffer*>(indexBuffer)->asVulkanBuffer();
		auto vertexBufferVK = reinterpret_cast<Buffer*>(vertexBuffer)->asVulkanBuffer();
		unsigned int indexSize = indexBuffer->stride;
		unsigned int vertexSize = vertexBuffer->stride;

		// Build Transforms
		std::vector<mat4> transforms_cpu;
		std::function<void(crossplatform::Mesh::SubNode&, size_t&, math::Matrix4x4&)> FillTransformsFromNode =
			[&](crossplatform::Mesh::SubNode& node, size_t& n, math::Matrix4x4& model)
		{
			const math::Matrix4x4& mat = node.orientation.GetMatrix();
			math::Matrix4x4 newModel;
			math::Multiply4x4(newModel, model, mat);
			model = newModel;
			mat4 m4;
			memcpy(&m4, newModel, sizeof(mat4));
			transforms_cpu.push_back(m4);
			for (int i = 0; i < node.children.size(); i++)
			{
				FillTransformsFromNode(node.children[i], n, model);
			}
			n++;
		};
		size_t nodeCount = 0;
		math::Matrix4x4 m = math::Matrix4x4::IdentityMatrix();
		FillTransformsFromNode(mesh->GetRootNode(), nodeCount, m);

		vulkanRenderPlatform->CreateVulkanBuffer(nullptr, transforms_cpu.size() * sizeof(mat4), 
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
			transformsBuffer, transformsBufferAllocation,"AccelerationStructure Transforms");
		void* mappedData = nullptr;
		SIMUL_VK_CHECK((vk::Result)vmaMapMemory(transformsBufferAllocation.allocator, transformsBufferAllocation.allocation, &mappedData));
		if (mappedData)
		{
			memcpy(mappedData, transforms_cpu.data(), transforms_cpu.size());
			vmaUnmapMemory(transformsBufferAllocation.allocator, transformsBufferAllocation.allocation);
		}

		// Build Geometry Descs
		std::function<void(crossplatform::Mesh::SubNode&, size_t&, size_t&)> FillGeometryFromNode =
			[&](crossplatform::Mesh::SubNode& node, size_t& n, size_t& g)
		{
			for (int i = 0; i < node.subMeshes.size(); i++)
			{
				crossplatform::Mesh::SubMesh* m = mesh->GetSubMesh(node.subMeshes[i]);
				if (!m)
					continue;

				vk::AccelerationStructureGeometryKHR geometryDesc;
				// Mark the geometry as opaque.
				// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
				// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
				geometryDesc.flags = vk::GeometryFlagBitsKHR::eOpaque;
				geometryDesc.geometryType = vk::GeometryTypeKHR::eTriangles;
				geometryDesc.geometry.triangles = vk::AccelerationStructureGeometryTrianglesDataKHR();
				geometryDesc.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
				geometryDesc.geometry.triangles.vertexData = device->getBufferAddress(vk::BufferDeviceAddressInfo(vertexBufferVK));
				geometryDesc.geometry.triangles.vertexStride = vertexSize;
				geometryDesc.geometry.triangles.maxVertex = vertexBuffer->count;
				geometryDesc.geometry.triangles.indexType = indexBuffer->stride == 4 ? vk::IndexType::eUint32 : vk::IndexType::eUint16;
				geometryDesc.geometry.triangles.indexData = device->getBufferAddress(vk::BufferDeviceAddressInfo(indexBufferVK)) + (uint64_t)m->IndexOffset * (uint64_t)indexBuffer->stride;
				geometryDesc.geometry.triangles.transformData = device->getBufferAddress(vk::BufferDeviceAddressInfo(transformsBuffer)) + sizeof(mat4) * n;
				m_Geometries.push_back(geometryDesc);
				m_PrimitiveCounts.push_back(static_cast<uint32_t>(m->TriangleCount));
				m_ASBRIs.push_back(vk::AccelerationStructureBuildRangeInfoKHR(static_cast<uint32_t>(m->TriangleCount), 0, 0, 0));
				g++;
			}
			for (int i = 0; i < node.children.size(); i++)
			{
				FillGeometryFromNode(node.children[i], n, g);
			}
			n++;
		};
		size_t meshCount = 0;
		nodeCount = 0;
		FillGeometryFromNode(mesh->GetRootNode(), nodeCount, meshCount);
	}
	else if (geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::AABB)
	{
		vk::Buffer pAABBResources[1] = {};
		size_t AABBCount = std::size(pAABBResources);
		pAABBResources[0] = *(reinterpret_cast<PlatformStructuredBuffer*>(aabbBuffer->platformStructuredBuffer)->GetLastBuffer());

		if (AABBCount == 0)
			return;

		// Build Geometry Descs
		std::function<void()> FillGeometryFromNode = [&]()
		{
			for (size_t i = 0; i < AABBCount; i++)
			{
				vk::AccelerationStructureGeometryKHR geometryDesc;
				geometryDesc.flags = vk::GeometryFlagBitsKHR(0);
				geometryDesc.geometryType = vk::GeometryTypeKHR::eAabbs;
				geometryDesc.geometry.aabbs = vk::AccelerationStructureGeometryAabbsDataKHR();
				geometryDesc.geometry.aabbs.data = device->getBufferAddress(vk::BufferDeviceAddressInfo(pAABBResources[i])) + (i * sizeof(Raytracing_AABB));
				geometryDesc.geometry.aabbs.stride = sizeof(Raytracing_AABB);
				m_Geometries.push_back(geometryDesc);
				m_PrimitiveCounts.push_back(static_cast<uint32_t>(aabbBuffer->count));
				m_ASBRIs.push_back(vk::AccelerationStructureBuildRangeInfoKHR(static_cast<uint32_t>(aabbBuffer->count), 0, 0, 0));
			}
		};
		FillGeometryFromNode();
	}
	else if (geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::UNKNOWN)
	{
		SIMUL_BREAK("ERROR: Provided geometry is BottomLevelAccelerationStructure::GeometryType::UNKNOWN");
	}
	else
	{
		SIMUL_BREAK("ERROR: BottomLevelAccelerationStructure::geometryType is invalid");
	}

	m_ASBGI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
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

	BuildBuffer(m_ASBSI.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, accelerationStructureBuffer, accelerationStructureBufferAllocation, "BLAS_AccelerationStructureBuffer");
	BuildBuffer(m_ASBSI.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer, scratchBuffer, scratchBufferAllocation, "BLAS_ScratchBuffer");

	vk::AccelerationStructureCreateInfoKHR asCI;
	asCI.createFlags;
	asCI.buffer = accelerationStructureBuffer;
	asCI.offset = 0;
	asCI.size = m_ASBSI.accelerationStructureSize;
	asCI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
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
