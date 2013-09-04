#ifndef SPHERICAL_HARMONICS_CONSTANTS_SL
#define SPHERICAL_HARMONICS_CONSTANTS_SL

uniform_buffer SphericalHarmonicsConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform float xxxxx;
};

struct SphericalHarmonicsSample
{ 
	float theta;
	float phi;
	float dummy1,dummy2;
	vec3 dir; 
	float dummy3;
	float coeff[16]; 
}; 

#endif