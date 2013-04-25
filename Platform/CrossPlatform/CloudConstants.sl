#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL
ALIGN cbuffer CloudConstants R0
{
	uniform vec3 ambientColour;
	uniform float cloud_interp;
	uniform vec3 fractalScale;
	uniform float cloudEccentricity;
	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float fadeInterp;
	uniform vec4 lightningMultipliers;
	uniform vec3 lightningColour;
	uniform float earthshadowMultiplier;
	uniform vec3 lightningSourcePos;
	uniform float rain;
	uniform vec2 screenCoordOffset;
};
#endif