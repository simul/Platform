#ifndef SIMULMATH_RANDOMNUMBERGENERATOR_H
#define SIMULMATH_RANDOMNUMBERGENERATOR_H
#include "Platform/Math/Export.h"
#include "Platform/Core/MemoryInterface.h"
namespace platform
{
	namespace math
	{
		class SIMUL_MATH_EXPORT RandomNumberGenerator
		{
			void *gnr;
			int last_seed;
		public:
			RandomNumberGenerator(platform::core::MemoryInterface *mem=NULL);
			virtual ~RandomNumberGenerator();
			void Seed(int RandomSeed);
			int GetSeed() const;
			float FRand(float minval=0.f,float maxval=1.f) const;
			unsigned URand(unsigned maximum) const;
			int IRand(int maximum) const;
			void RandomDirection(float &x,float &y,float &z);
			void SphericalRandom(float &x,float &y,float &z);
			void RandomAngle(float &az,float &el);
			platform::core::MemoryInterface *memoryInterface;
		};
	}
}

#endif // NOISE_H
