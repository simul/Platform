#ifndef HDR_CONSTANTS_SL
#define HDR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(HdrConstants,12)
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec2 offset;
	uniform float nearZ,farZ;
	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float gamma;
	uniform vec3 depthToLinFadeDistParams;
	uniform float padHdrConstants;
	uniform vec4 warpHmdWarpParam;
	uniform vec2 lowResTexelSizeX;
	uniform vec2 warpLensCentre;
	uniform vec2 warpScreenCentre;
	uniform vec2 warpScale;
	uniform vec2 warpScaleIn;
	uniform vec2 padHdrConstants2;
	uniform vec4 hiResToLowResTransformXYWH;
SIMUL_CONSTANT_BUFFER_END
	
SIMUL_CONSTANT_BUFFER(rectConstants,11)
	uniform vec4 rect;
SIMUL_CONSTANT_BUFFER_END
	
SIMUL_CONSTANT_BUFFER(TestHdrConstants,5)
	uniform vec4 colour;
SIMUL_CONSTANT_BUFFER_END
#endif