//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_2D_CLOUD_CONSTANTS_SL
#define SIMUL_2D_CLOUD_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(Cloud2DConstants,11)
	uniform vec4 viewportToTexRegionScaleBias;
	uniform mat4 worldViewProj;
	uniform mat4 invViewProj;
	uniform mat4 viewProj;
	uniform mat4 worldToScatteringVolumeMatrix;
	uniform vec4 mixedResTransformXYWH;

	uniform vec3 origin;
	uniform float globalScale;

	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float cloudEccentricity;

	uniform vec3 ambientLight;
	uniform float extinction;

	uniform vec3 eyePosition;
	uniform float maxFadeDistanceMetres;

	uniform vec3 sunlightX;
	uniform float cloudInterp;

	uniform vec3 mieRayleighRatio;
	uniform float hazeEccentricity;

	uniform float detailScale;
	uniform float planetRadius;
	uniform float fractalWavelength;
	uniform float fractalAmplitude;
	
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float detailDensity;

	uniform vec4 depthToLinFadeDistParams;

	uniform float time;
	uniform float maxAltitudeMetres;
	uniform float offsetScale;
	uniform float maxCloudDistanceMetres;
	uniform int2 offsetToCorner;
	uniform int2 targetTextureSize1;
	uniform vec3 infraredIntegrationFactors;
	uniform float cloudAltitudeKm;
	uniform vec3 cloudIrRadiance;
	uniform float exposure;
	
	uniform int4 targetRange;
	uniform int2 edge;
	uniform uint2 amortizationScale;

	uniform uint2 amortizationOffset;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(Detail2DConstants,12)
	uniform float persistence;
	uniform int octaves;
	uniform float amplitude;
	uniform float density;						// Uniformly distributed thickness/cloudiness

	uniform vec3 lightDir2d;
	// for coverage
	uniform float coverageOctaves;

	uniform float coveragePersistence;
	uniform float humidity;
	uniform float coverageDiffusivity;
	uniform float noiseTextureScale;			// Scale from existing random texture to noise scale of coverage.

	uniform float phase;
	uniform float detailTextureSize;
	uniform float detailDiffusivity;
	uniform float detailOctaves;
SIMUL_CONSTANT_BUFFER_END

#endif