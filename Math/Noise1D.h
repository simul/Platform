#ifndef SIMUL_MATH_NOISE1D_H
#define SIMUL_MATH_NOISE1D_H
#include "Platform/Math/Export.h"
#include "Platform/Math/Noise3D.h"

namespace simul
{
	namespace math
	{
		//! A 3D Perlin noise class.
		SIMUL_MATH_EXPORT_CLASS Noise1D
		{
			unsigned frequency;
			float *noise_buffer;
			float value_at(unsigned i) const;
			float noise1(float p) const;
			int numOctaves;
			float persistence;
			NoiseFilter *filter;
			unsigned buffer_size;
			simul::base::MemoryInterface *memoryInterface;
			RandomNumberGenerator *noise_random;
			int generation_number;
		public:
			Noise1D(simul::base::MemoryInterface *mem=NULL);
			virtual ~Noise1D();
		//! Define the grid of pseudo-random numbers to be used in the PerlinNoise3D function. The parameter freq is the
		//! frequency, or grid-size.
			void Setup(unsigned freq,int RandomSeed,int octaves,float persistence=.5f);
			int GetNoiseFrequency() const;
			float PerlinNoise1D(float x) const;
			void SetFilter(NoiseFilter *nf);
		// MemoryUsageInterface
			virtual unsigned GetMemoryUsage() const;
			virtual int GetGenerationNumber() const
			{
				return generation_number;
			}
		};
	}
}

#endif // NOISE_H
