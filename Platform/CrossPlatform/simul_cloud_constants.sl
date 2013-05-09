#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL

uniform_buffer CloudConstants R9
{
	uniform mat4 worldViewProj;
	uniform mat4 wrld;

	uniform mat4 noiseMatrix;
	uniform vec3 inverseScales;
	uniform float x2;
	uniform vec3 ambientColour;
	uniform float cloud_interp;
	uniform vec3 fractalScale;
	uniform float cloudEccentricity;
	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float earthshadowMultiplier;
	uniform vec3 cornerPos;
	uniform float hazeEccentricity;
	uniform vec4 lightningMultipliers;
	uniform vec4 lightningColour;
	uniform vec3 lightningSourcePos;
	uniform float rain;
	uniform vec3 sunlightColour1;
	uniform float x3;
	uniform vec3 sunlightColour2;
	uniform float x4;
	uniform vec2 screenCoordOffset;
	uniform vec2 y1;
	uniform vec3 mieRayleighRatio;
	uniform float alphaSharpness;
	uniform vec3 illuminationOrigin;
	uniform float maxFadeDistanceMetres;
	uniform vec3 illuminationScales	;
	uniform float x5;
	uniform vec3 crossSectionOffset	;
	uniform float x6;
};
#endif