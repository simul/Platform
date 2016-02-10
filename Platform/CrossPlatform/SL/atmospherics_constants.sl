//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef ATMOSPHERICS_CONSTANTS_SL
#define ATMOSPHERICS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(AtmosphericsUniforms,11)
	uniform vec3 lightDir;
	uniform float pad1;
	uniform vec3 mieRayleighRatio;
	uniform float pad1a;
	uniform vec2 texelOffsetsX;
	uniform float hazeEccentricity;
	uniform float cloudShadowing;
	// X, Y and Z for the bottom-left corner of the cloud shadow texture.
	uniform vec3 cloudOrigin;
	uniform float cloudShadowSharpness;
	uniform vec3 cloudScale;
	uniform float pad8;

	uniform float overcast;
	uniform float maxFadeDistanceMetres;
	uniform float aejarjt;
	uniform float fogBaseAlt;

	uniform vec3 fogColour;
	uniform float fogScaleHeight;
    uniform vec3 infraredIntegrationFactorsUNUSED;
	uniform float fogDensity;
    uniform vec3 yAxisXXX;
	uniform float pad9;
    uniform vec3 xAxisXXX;
	uniform float pad10;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(AtmosphericsPerViewConstants,12)
	uniform mat4 lightspaceToWorldMatrix;
	uniform mat4 invViewProj;
	uniform mat4 invShadowMatrix;
	
	uniform mat4 worldToCloudMatrix;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec4 mixedResolutionTransformXYWH;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec3 viewPosition;
	uniform float exposure;
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float farZ;
	
    uniform uint3 scatteringVolumeDims;
	uniform float godraysIntensity;
	
	uniform uint3 amortizationOffset;
	uniform float startZMetresZZZ;

	uniform uint3 amortization;
	uniform float test;

	uniform int2 offsetToCorner;
	uniform uint2 edge;

SIMUL_CONSTANT_BUFFER_END

#endif
