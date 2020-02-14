#include <math.h>
#include <stdlib.h>
#include "Simul/Platform/Math/RandomNumberGenerator.h"
#include "Simul/Platform/Math/mmxrand.h"
#include "Simul/Base/ConvertPreprocessorMacros.h"

#ifdef _MSC_VER
	// C4512 - "assignment operator could not be generated". Boost classes cause this warning.
	// Also C4127 - "conditional expression is constant"
	#pragma warning(push)
	#pragma warning(disable:4512)
	#pragma warning(disable:4127)
#endif

#ifndef _CPPUNWIND
#define throw
#endif


#ifdef SIMUL_USE_CPP11_RANDOMS
#include <random>
using std::mt19937;
typedef std::uniform_real_distribution<float> distribution_type;
typedef std::uniform_int_distribution<int> int_distribution_type;
static distribution_type distribution(0,1.f);
#endif

#ifdef _MSC_VER
#pragma optimize("",off)
#endif

using namespace simul::math;

simul::math::RandomNumberGenerator::RandomNumberGenerator(simul::base::MemoryInterface *mem)
	:gnr(NULL)
	,last_seed(-1)
	,memoryInterface(mem)
{
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=NULL;
	generator=new(memoryInterface) mt19937;
	gnr=generator;
#else
	gnr=(void*)(new MMXTwister());
#endif
}

simul::math::RandomNumberGenerator::~RandomNumberGenerator()
{
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=(mt19937*)gnr;
	del(memoryInterface,generator);
#else
	MMXTwister *t=(MMXTwister *)(gnr);
	delete t;
#endif
}

float simul::math::RandomNumberGenerator::FRand(float minval,float maxval) const
{
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=(mt19937*)gnr;
	float r=distribution(*generator);
	return r;
#else
	//double r=rand()/(double)RAND_MAX;
	MMXTwister *t=(MMXTwister *)(gnr);
	double r=t->rand();
	return  minval+(maxval-minval)*(float)r;
#endif
}

unsigned simul::math::RandomNumberGenerator::URand(unsigned maximum) const
{
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=(mt19937*)gnr;
	int_distribution_type int_distribution(0, maximum);
	int r=int_distribution(*generator);
	return r;
#else
	//double r=rand()/(double)RAND_MAX;
	MMXTwister *t=(MMXTwister *)(gnr);
	unsigned r=t->randInt()%maximum;
	return  r;
#endif
}

int simul::math::RandomNumberGenerator::IRand(int maximum) const
{
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=(mt19937*)gnr;
	int_distribution_type int_distribution(0, maximum);
	int r=int_distribution(*generator);
	return r;
#else
	//double r=rand()/(double)RAND_MAX;
	MMXTwister *t=(MMXTwister *)(gnr);
	unsigned r=t->randInt()%maximum;
	return  r;
#endif
}
 
void simul::math::RandomNumberGenerator::Seed(int RandomSeed)
{
	last_seed=RandomSeed;
#ifdef SIMUL_USE_CPP11_RANDOMS
	mt19937 *generator=(mt19937*)gnr;
	generator->seed(last_seed);
#else
	//srand(last_seed);
	// 1st random is always really small!
	//rand();
	MMXTwister *t=(MMXTwister *)(gnr);
	t->Seed(last_seed);
#endif
}

int simul::math::RandomNumberGenerator::GetSeed() const
{
	return last_seed;
}

void simul::math::RandomNumberGenerator::RandomDirection(float &x,float &y,float &z)
{
	float az=0,el=0;
	RandomAngle(az,el);
	x=(float)(sin(az)*cos(el));
	y=float(cos(az)*cos(el));
	z=float(sin(el));
}

void simul::math::RandomNumberGenerator::SphericalRandom(float &x,float &y,float &z)
{
	float r=1.f-(float)pow(FRand(),4.f);
	float az=FRand()*2.0f*3.1415926536f;
	float el=(float)asin(FRand()*2.f-1.f);
	x=r*(float)sin(az)*(float)cos(el);
	y=r*(float)cos(az)*(float)cos(el);
	z=r*(float)sin(el);
}

void simul::math::RandomNumberGenerator::RandomAngle(float &az,float &el)
{
	az=FRand()*2*3.1415926536f;
	el=(float)asin(FRand()*2.f-1.f);
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
