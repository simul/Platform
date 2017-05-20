//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SPHERICAL_HARMONICS_CONSTANTS_SL
#define SPHERICAL_HARMONICS_CONSTANTS_SL

#ifndef MAX_SH_BANDS
#define MAX_SH_BANDS 4
#endif
#ifndef MAX_SH_SAMPLES
#define MAX_SH_SAMPLES 16
#endif
#ifndef MAX_SH_COEFFICIENTS
#define MAX_SH_COEFFICIENTS 16
#endif

SIMUL_CONSTANT_BUFFER(SphericalHarmonicsConstants,10)
	uniform int num_bands;		// The range of the parameter m is from 0 to num_bands-1
	uniform int sqrtJitterSamples;
	uniform int numJitterSamples;
	uniform float invNumJitterSamples; 
	uniform int randomSeed;
	uniform int numCoefficients;
SIMUL_CONSTANT_BUFFER_END

struct SphericalHarmonicsSample
{ 
	vec3 dir; 
	float dummy3;
	vec4 lookup;
	float coeff[MAX_SH_BANDS*MAX_SH_BANDS]; 
}; 

#endif