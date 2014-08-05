#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float nearZ,farZ;
	uniform vec4 depthToLinFadeDistParams;
	uniform uint2 source_dims;
	uniform uint2 target_dims;
	uniform uint2 source_offset;
	//uniform vec4 mixedResolutionTransformXYWH; Not needed
SIMUL_CONSTANT_BUFFER_END

#endif