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

	uniform float overcast_deprecated;
	uniform float maxFadeDistanceMetres;
	uniform float aejarjt;
	uniform float fogBaseAlt;

	uniform vec3 fogColour;
	uniform float fogScaleHeight;
    uniform vec3 atmosphericsDirectionToSun;
	uniform float atmosphericsPlanetRadiusKm;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(AtmosphericsPerViewConstants,12)
	uniform mat4 lightspaceToWorldMatrix;
	uniform mat4 scatterspaceToWorldMatrix;
	uniform mat4 worldToCloudMatrix;
	uniform vec3 cloudOriginKm;
	uniform float padXXXXXXX;
	uniform vec3 cloudScaleKm;
	uniform float padXXXXXXXX;

	uniform vec3 viewPosition;
	uniform float zOrigin;
	
    uniform uint3 scatteringVolumeDims;
	uniform float godraysIntensity;

    uniform uint3 godraysVolumeDims;
	uniform float q35uj64jwq;
	
	uniform uint3 amortizationOffset;
	uniform float startZMetresZZZ;

	uniform uint3 amortization;
	uniform float testXXXX;

	uniform uint2 edge;

SIMUL_CONSTANT_BUFFER_END

#endif
