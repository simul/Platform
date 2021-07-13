#ifndef VirtualVectorH
#define VirtualVectorH

#include "SimVector.h"

namespace simul
{
	namespace math
	{
class Vector3;

/// \internal A vector in arbitrary memory.

/// VirtualVector is derived from Vector but does not manage its own data. Instead
/// the #PointTo method is used to give vector functionality to floating point data
/// anywhere in memory (as long as any alignment conditions are satisfied.)
class SIMUL_MATH_EXPORT VirtualVector: public Vector
{
public:
	VirtualVector(void);
	~VirtualVector(void);
	VirtualVector(unsigned psize){size=psize;}  
	VirtualVector(unsigned psize,float *addr){size=psize;PointTo(addr);}
	virtual void operator=(const Vector &V);
	void operator=(const class Vector3 &v);
	void PointTo(float *addr);	///< Use the data at address addr for this vector.
	void Resize(unsigned sz);		///< Make the vector be size sz.
	virtual void operator*=(float f);             
	virtual void Zero(void);
	friend void SIMUL_MATH_EXPORT_FN Add(Vector3 &R,const VirtualVector &X1,const VirtualVector &X2);
	friend void SIMUL_MATH_EXPORT_FN Subtract(VirtualVector &R,const Vector3 &X1,const Vector3 &X2);
	friend void SIMUL_MATH_EXPORT_FN Subtract(Vector3 &R,const Vector3 &X1,const VirtualVector &X2);
	friend void SIMUL_MATH_EXPORT_FN CrossProduct(Vector3 &result,const VirtualVector &v1,const VirtualVector &v2);
};
}
}
#endif
