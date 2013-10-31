#ifndef HDR_CONSTANTS_SL
#define HDR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(HdrConstants,9)
	uniform vec4 rect;
	uniform vec2 offset;
	uniform float nearZ,farZ;
	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float gamma;
	uniform vec3 depthToLinFadeDistParams;
	uniform float padHdrConstants;
	uniform vec2 lowResTexelSize;
SIMUL_CONSTANT_BUFFER_END


#endif