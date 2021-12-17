//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SPHERE_CONSTANTS_SL
#define SPHERE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SphereConstants,7)
	uniform mat4 debugWorldViewProj;
	uniform vec4 rect;
	uniform vec4 multiplier;
	uniform vec4 quaternion;
	uniform vec4 quaternion2;

	uniform uint latitudes;
	uniform uint longitudes;
	uniform float radius;
	uniform float sideview;

	uniform vec4 debugColour;
	
	uniform vec3 debugViewDir;
	uniform int slices;

	uniform vec2 texcOffset;
SIMUL_CONSTANT_BUFFER_END
#endif
