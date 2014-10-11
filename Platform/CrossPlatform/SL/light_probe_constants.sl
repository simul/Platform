#ifndef LIGHT_PROBE_CONSTANTS_SL
#define LIGHT_PROBE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightProbeConstants,9)
	uniform mat4 invViewProj;
	uniform int numSHBands;
	uniform float alpha;
	uniform float LightProbeConstantsB;
	uniform float LightProbeConstantsC;
SIMUL_CONSTANT_BUFFER_END

#endif