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
	uniform vec4 warpHmdWarpParam;
	uniform vec2 lowResTexelSize;
	uniform vec2 warpLensCentre;
	uniform vec2 warpScreenCentre;
	uniform vec2 warpScale;
	uniform vec2 warpScaleIn;
	uniform vec2 padHdrConstants2;
SIMUL_CONSTANT_BUFFER_END


#endif