#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float nearZ,farZ;
	uniform vec3 depthToLinFadeDistParams;
	uniform float ccc;
SIMUL_CONSTANT_BUFFER_END

#endif