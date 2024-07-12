#pragma once
#include "Platform/Core/RuntimeError.h"

namespace platform
{
	namespace math
	{
		bool IsPowerOfTwo(uint64_t n)
		{
			return ((n & (n - 1)) == 0 && (n) != 0);
		}

		uint64_t NextMultiple(uint64_t value, uint64_t multiple)
		{
			SIMUL_ASSERT(IsPowerOfTwo(multiple));

			return (value + multiple - 1) & ~(multiple - 1);
		}
	}
}