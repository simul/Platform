#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float nearZ,farZ;
	uniform vec4 depthToLinFadeDistParams;
	uniform uint2 source_dims;
	uniform uint2 target_dims;
	uniform uint2 source_offset;	// The offset into the area of the texture that's of interest
	uniform uint2 cornerOffset;		// The offset from the top-left of the low-res texture to the top-left of the hi-res rea.
	//uniform vec4 mixedResolutionTransformXYWH; Not needed
SIMUL_CONSTANT_BUFFER_END

#endif