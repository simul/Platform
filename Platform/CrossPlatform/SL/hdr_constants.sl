//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef HDR_CONSTANTS_SL
#define HDR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(HdrConstants,12)
	uniform mat4 worldToScatteringVolumeMatrix;
	uniform mat4 invViewProj;
	uniform mat4 invShadowMatrix;
	uniform vec4 viewportToTexRegionScaleBias;

	uniform vec2 offset;
	uniform float alpha;
	uniform float cloudShadowStrength;
	
	uniform vec4 colour2;

	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float gamma;

	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 warpHmdWarpParam;

	uniform vec2 lowResTexelSizeX;
	uniform vec2 warpLensCentre;

	uniform vec2 warpScreenCentre;
	uniform vec2 warpScale;

	uniform vec2 warpScaleIn;
	uniform vec2 padHdrConstants1;

	uniform uint2 hiResDimsX;
	uniform uint2 lowResDims;

	uniform uint2 fullResDims;
	uniform int numSamples;
	uniform float maxFadeDistanceKm;

	uniform vec3 infraredIntegrationFactors;
	uniform int randomSeed;

	uniform vec3 viewPos;
	uniform float nearDist;					// near threshold for discarding depths.
SIMUL_CONSTANT_BUFFER_END
	
#endif