//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef LIGHT_PROBE_CONSTANTS_SL
#define LIGHT_PROBE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightProbeConstants,9)
	uniform mat4 invViewProj;
	uniform int cubeFace;
	uniform int numSHBands;
	uniform int mipIndex;
	uniform int numMips;
	uniform float alpha;
	uniform float roughness;
	uniform vec2 padding;
SIMUL_CONSTANT_BUFFER_END

#endif