#ifndef SPHERICAL_HARMONICS_CONSTANTS_SL
#define SPHERICAL_HARMONICS_CONSTANTS_SL

#ifndef MAX_SH_BANDS
#define MAX_SH_BANDS (8)
#endif

uniform_buffer SphericalHarmonicsConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform int num_bands;		// The range of the parameter m is from 0 to num_bands-1
	uniform int sqrtJitterSamples;
	uniform int numJitterSamples;
	uniform float invNumJitterSamples;
};

struct SphericalHarmonicsSample
{ 
	vec3 dir; 
	float dummy3;
	float coeff[MAX_SH_BANDS*MAX_SH_BANDS]; 
}; 

#endif