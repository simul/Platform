#ifndef SIMULMATH_RANDOMNUMBERGENERATOR_H
#define SIMULMATH_RANDOMNUMBERGENERATOR_H
#include "Simul/Platform/Math/Export.h"
#include "Simul/Base/MemoryInterface.h"
namespace simul
{
	namespace math
	{
		SIMUL_MATH_EXPORT_CLASS RandomNumberGenerator
		{
			void *gnr;
			int last_seed;
		public:
			RandomNumberGenerator(simul::base::MemoryInterface *mem=NULL);
			virtual ~RandomNumberGenerator();
			void Seed(int RandomSeed);
			int GetSeed() const;
			float FRand(float minval=0.f,float maxval=1.f) const;
			unsigned URand(unsigned maximum) const;
			int IRand(int maximum) const;
			void RandomDirection(float &x,float &y,float &z);
			void SphericalRandom(float &x,float &y,float &z);
			void RandomAngle(float &az,float &el);
			simul::base::MemoryInterface *memoryInterface;
		};
	}
}

#endif // NOISE_H
