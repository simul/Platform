//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef DEBUG_CONSTANTS_SL
#define DEBUG_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(DebugConstants,7)
	uniform mat4 debugWorldViewProj;
	uniform vec4 rect;
	uniform vec4 multiplier;
	uniform vec4 quaternion;

	uniform uint latitudes;
	uniform uint longitudes;
	uniform float radius;
	uniform float sideview;

	uniform vec4 viewport;
	uniform vec4 debugColour;
	uniform vec4 debugDepthToLinFadeDistParams;

	uniform vec4 debugTanHalfFov;

	uniform uint4 texSize;	// xy, z if needed, w=array size

	uniform uint2 queryPos;	// for texture queries.
	uniform float debugGamma;
	uniform float debugDepth;

	uniform float displayMip;
	uniform uint displayLayer;
	uniform float debugTime;
	uniform float dc_pad1;

	uniform vec3 line_start;
	uniform float dc_pad2;

	uniform vec3 line_end;
	uniform float dc_pad3;
SIMUL_CONSTANT_BUFFER_END
#endif
