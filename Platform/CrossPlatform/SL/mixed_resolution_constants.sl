#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float nearZ,farZ;
	uniform vec3 depthToLinFadeDistParams;
	uniform float ccc;
	uniform uint2 source_dims;
	uniform float ddd,eee;
	//uniform vec4 mixedResolutionTransformXYWH; Not needed
SIMUL_CONSTANT_BUFFER_END

#endif