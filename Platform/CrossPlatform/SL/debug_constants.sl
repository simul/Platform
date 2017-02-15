//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef DEBUG_CONSTANTS_SL
#define DEBUG_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(DebugConstants,8)
	uniform mat4 debugWorldViewProj;
	uniform vec4 rect;
	uniform vec4 multiplier;

	uniform int latitudes;
	uniform int longitudes;
	uniform float radius;
	uniform float sideview;

	uniform vec4 viewport;
	uniform vec4 debugColour;
	uniform vec4 debugDepthToLinFadeDistParams;

	uniform vec4 debugTanHalfFov;

	uniform uint4 texSize;	// xy, z if needed, w=array size

	uniform float displayLod;
	uniform float displayLevel;

	uniform uint2 queryPos;	// for texture queries.
	uniform float debugGamma;
	uniform float debugExposureXXX;
	
	uniform vec4 quaternion;
	uniform vec3 debugViewDir;
SIMUL_CONSTANT_BUFFER_END
#endif
