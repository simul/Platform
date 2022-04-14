#ifndef SIMUL_MATH_GEOMETRICFUNCTIONS_H
#define SIMUL_MATH_GEOMETRICFUNCTIONS_H
#include "Export.h"
namespace platform
{
	namespace math
	{
		class Vector3;
		extern void SIMUL_MATH_EXPORT_FN SupportPoint(float *PointsX,float *PointsY,float *PointsZ,const Vector3 &D,int NumPoints,Vector3 &X1,int &i1);
	}
}

#endif
