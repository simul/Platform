#define SIM_MATH
#include "MathFunctions.h"
#include <math.h>     
#include <algorithm>

namespace platform
{
	namespace math
	{
float Sine(float x)
{
	return sinf(x);
}
          
float Cosine(float x)
{
	return (float)cos(x);
} 

float InverseTangent(float y,float x)
{
	return (float)atan2((float)y,(float)x);
}
float InverseCosine(float x)
{
	return acosf(x);
}
class BadNumber{};
float Sqrt(float x)
{
	return sqrtf(x);
}
void IntegerPowerOfTen(float num,float &man,int &Exp)
{
	if(fabs(num)<1e-8f)
		num=1;
	float expf=logf(num);
	expf/=logf(10.f);
	if(expf>=0)
		Exp=(int)(expf+0.5f);
	else
		Exp=(int)(expf-0.5f);
	float powr=powf(10.f,(float)Exp);
	man=num/powr;

}    
void PowerOfTen(float num,float &Exp)
{
	if(fabs(num)<1e-8f)
		num=1.f;
	Exp=(float)log(num);
	Exp/=(float)log(10.f);
}

}
}
