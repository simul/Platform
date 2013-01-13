#ifndef CLOUD_CONSTANTS_GLSL
#define CLOUD_CONSTANTS_GLSL
layout(std140) uniform CloudConstants
{
	uniform vec3 ambientColour;
	uniform float cloud_interp;
	uniform vec3 fractalScale;
	uniform float cloudEccentricity;
	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float fadeInterp;
	uniform vec4 lightningMultipliers;
	uniform vec4 lightningColour;
	uniform vec2 screenCoordOffset;
	uniform float earthshadowMultiplier;
	uniform float xxxx;
};
#endif