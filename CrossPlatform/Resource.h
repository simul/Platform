#pragma once


namespace platform
{
	namespace crossplatform
	{
		static const int PER_PASS_RESOURCE_GROUP=3;
		//! The grouping for shader resources. Using this grouping we can ensure that
		//! resource layouts are not uploaded more often than necessary. See ConstantBuffer<>.
		enum class ResourceUsageFrequency
		{
			ONCE = 1,
			ONCE_PER_FRAME = 2,
			FEW_PER_FRAME = 3,
			MANY_PER_FRAME = 4,
			OCTAVE_1 = ONCE,
			OCTAVE_2 = ONCE_PER_FRAME,
			OCTAVE_3 = FEW_PER_FRAME,
			OCTAVE_4 = MANY_PER_FRAME
		};
		//! The layout of a group of resources. For now this refers only to constant buffers.
		//! You may see this referred to as a "Descriptor Set" layout elsewhere.
		struct ResourceGroupLayout
		{
			//! Bitmask for which constant buffer slots are in this group.
			uint64_t constantBufferSlots=0;
			inline void UseConstantBufferSlot(uint8_t s)
			{
				constantBufferSlots |= uint64_t(1) << uint64_t(s);
			}
			inline bool UsesConstantBufferSlot(uint8_t s) const
			{
				uint64_t b = uint64_t(1) << uint64_t(s);
				uint64_t present = b & constantBufferSlots;
				return present!=uint64_t(0);
			}
			inline uint8_t GetNumConstantBuffers() const
			{
				uint8_t count = 0;
				uint64_t slotsRemaining = constantBufferSlots;
				for(uint8_t s=0;s<64;s++)
				{
					uint64_t b = uint64_t(1) << uint64_t(s);
					uint64_t present = b & constantBufferSlots;
					if(present != uint64_t(0))
						count++;
					slotsRemaining&=(~b);
					if(!slotsRemaining)
						break;
				}
				return count;
			}
			
		};
	}
}