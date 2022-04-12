#pragma once
#include "Export.h"
#include "Quaterniond.h"
namespace platform
{
	namespace crossplatform
	{
		enum class AxesStandard
		{
			NotInitialized = 0,
			RightHanded = 1,
			LeftHanded = 2,
			YVertical = 4,
			Engineering = 8 | RightHanded,
			OpenGL = 16 | RightHanded,
			Unreal = 32 | LeftHanded,
			Unity = 64 | LeftHanded | YVertical,
		};

		inline AxesStandard operator|(const AxesStandard& a, const AxesStandard& b)
		{
			return (AxesStandard)((unsigned)a | (unsigned)b);
		}

		inline AxesStandard operator&(const AxesStandard& a, const AxesStandard& b)
		{
			return (AxesStandard)((unsigned)a & (unsigned)b);
		}
		extern mat4 SIMUL_CROSSPLATFORM_EXPORT ConvertMatrix(AxesStandard fromStandard, AxesStandard toStandard, const mat4& m);
		extern int8_t SIMUL_CROSSPLATFORM_EXPORT ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis);
		extern Quaternionf SIMUL_CROSSPLATFORM_EXPORT ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard,const Quaternionf& rotation);
		extern vec3 SIMUL_CROSSPLATFORM_EXPORT ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, const vec3& position);
		extern vec3 SIMUL_CROSSPLATFORM_EXPORT ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, const vec3& scale);
	}
}