//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef MIXED_RESOLUTIONS_CONSTANTS_SL
#define MIXED_RESOLUTIONS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint4 cubeIndex[8];
	uniform uint4 drawRange[8];
	uniform uint4 targetRange[8];
	uniform mat4 invViewProj[8];		// Transform from render clip to true view direction
	uniform mat4 viewProj;				// Transform from worldspace view direction to current frustum
	uniform mat4 viewProj_alternateEye;	// Transform from worldspace view dir to alternate eye's frustum.
	uniform mat4 projUNUSED;					// Transform from viewspace view direction to current frustum
	uniform vec4 frustumClipRange;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 tanHalfFov;
	uniform uint2 source_dims;
	uniform uint2 max_dims;

	uniform uint2 target_dims;
	uniform uint2 source_offsetUNUSED;	// The offset into the area of the texture that's of interest

	uniform uint2 scale;
	uniform vec2 texelRange;

	uniform vec4 depthWindow[2];		// xy and wh of the window of the depth texture that represents the current view.
	
	uniform vec2 stochasticOffset;
	uniform vec2 tanHalfFovUnused;
	uniform float nearThresholdDepth;
	uniform float nearThresholdDist;
SIMUL_CONSTANT_BUFFER_END

#endif