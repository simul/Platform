//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef LIGHT_PROBE_CONSTANTS_SL
#define LIGHT_PROBE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightProbeConstants,9)
	uniform mat4 invViewProj;
	uniform int numSHBands;
	uniform float alpha;
	uniform int cubeFace;
	uniform int mipIndex;
	uniform int numMips;
	uniform float roughness;
SIMUL_CONSTANT_BUFFER_END

#endif