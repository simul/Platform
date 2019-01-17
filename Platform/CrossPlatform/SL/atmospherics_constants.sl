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
	uniform float maxFadeDistanceMetres;

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
	uniform float averageCoverage;

	uniform uint2 edge;
	uniform vec2 padding;

SIMUL_CONSTANT_BUFFER_END

#endif
