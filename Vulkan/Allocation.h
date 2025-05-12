#pragma once
#include "Platform/CrossPlatform/Resource.h"
#include "vk_mem_alloc.h"

namespace platform::vulkan
{
	struct AllocationInfo
	{
		VmaAllocator allocator;			  // The allocator used to create the allocation.
		VmaAllocation allocation;		  // VMA's internal allocation reference.
		VmaAllocationInfo allocationInfo; // Information about the alloaction.
	};
}