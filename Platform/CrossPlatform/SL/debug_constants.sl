#ifndef DEBUG_CONSTANTS_SL
#define DEBUG_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(DebugConstants,8)
	uniform mat4 worldViewProj;

	uniform int latitudes,longitudes;
	uniform float radius;
	uniform float multiplier;

	uniform vec4 viewport;
	uniform vec4 colour;
	uniform vec4 depthToLinFadeDistParams;

	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float gamma;
SIMUL_CONSTANT_BUFFER_END
#endif
