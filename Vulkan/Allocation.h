#pragma once
#include "vk_mem_alloc.h"

struct AllocationInfo
{
	VmaAllocator allocator;			  // The allocator used to create the allocation.
	VmaAllocation allocation;		  // VMA's internal allocation reference.
	VmaAllocationInfo allocationInfo; // Information about the alloaction.
};