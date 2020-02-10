#pragma once
#include <float.h>
namespace simul
{
	namespace math
	{
		inline bool is_finite(float f)
		{
		#ifdef _MSC_VER
				if(_isnan(f)||!_finite(f))
		#else
				if(isnan(f)||std::isinf(f))
		#endif
					return false;
				return true;
		}
	}
}