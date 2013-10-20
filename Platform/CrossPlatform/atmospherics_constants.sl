#ifndef ATMOSPHERICS_CONSTANTS_SL
#define ATMOSPHERICS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(AtmosphericsUniforms,11)
	uniform vec3 lightDir;
	uniform float pad1;
	uniform vec3 mieRayleighRatio;
	uniform float pad1a;
	uniform vec2 texelOffsets;
	uniform float hazeEccentricity;
	uniform float pad6;
	// X, Y and Z for the bottom-left corner of the cloud shadow texture.
	uniform vec3 cloudOrigin;
	uniform float pad7;
	uniform vec3 cloudScale;
	uniform float pad8;

	uniform float overcast;
	uniform float maxFadeDistanceMetres;
	uniform float exposure;
	uniform float fogBaseAlt;

	uniform vec3 fogColour;
	uniform float fogScaleHeight;
    uniform vec3 infraredIntegrationFactors;
	uniform float fogDensity;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(AtmosphericsPerViewConstants,12)
	uniform mat4 invViewProj;
	uniform mat4 invShadowMatrix;
	uniform mat4 shadowMatrix;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 viewPosition;
	uniform float pad9;
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float farZ;
	uniform vec3 depthToLinFadeDistParams;
	uniform float startZMetres;
    uniform vec2 depthPixelScales;
	uniform float shadowRange;

	uniform float AtmosphericsPerViewConstantspad2;
SIMUL_CONSTANT_BUFFER_END

#endif
