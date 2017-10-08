//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef LIGHT_PROBE_CONSTANTS_SL
#define LIGHT_PROBE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightProbeConstants,9)
	uniform mat4 invViewProj;
	uniform int numSHBands;
	uniform float alpha;
	uniform float copy_face_exposure;
	uniform float copy_face_gamma;
	uniform int mip;
	uniform int num_mips;

SIMUL_CONSTANT_BUFFER_END

#endif