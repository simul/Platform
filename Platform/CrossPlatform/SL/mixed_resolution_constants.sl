//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform mat4 invViewProj;		// Transform from render clip to true view direction
	uniform mat4 viewProj;			// Transform from view direction to current frustum
	uniform vec4 frustumClipRange;
	uniform vec4 depthToLinFadeDistParams;
	uniform uint2 source_dims;
	uniform uint2 max_dims;

	uniform uint2 target_dims;
	uniform uint2 source_offset;	// The offset into the area of the texture that's of interest
	uniform int4 targetRange;

	uniform uint2 scale;
	uniform vec2 texelRange;

	uniform vec4 lowResToHighResTransformXYWH;
	
	uniform vec2 stochasticOffset;
	uniform vec2 tanHalfFov;
	uniform float nearThresholdDepth;
	uniform float wq084h23g23gt;

	uniform int cubeIndex;
	uniform float farZ;
SIMUL_CONSTANT_BUFFER_END

#endif