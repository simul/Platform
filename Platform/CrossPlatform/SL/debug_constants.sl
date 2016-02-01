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

	uniform vec2 debugTanHalfFov;
	uniform float debugExposure;
	uniform float debugGamma;
SIMUL_CONSTANT_BUFFER_END
#endif
