//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SKY_CONSTANTS_SL
#define SKY_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SkyConstants,10)
	uniform mat4 worldViewProj;
	uniform mat4 proj;
	
	uniform float altitudeTexCoord;
	uniform float hazeEccentricity;
	uniform float skyInterp;
	uniform float starBrightness;

	uniform mat4 invViewProj;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec4 eyePosition;
	uniform vec4 lightDir;
	uniform vec4 mieRayleighRatio;
	uniform vec4 colour;

	uniform float radiusRadians;
	uniform float isForwardDepth;
	uniform float glowRadiusMultiple;
	uniform uint numQueries;
	
	uniform uint3 threadOffset;
	uniform float maxFadeDistanceKm;

	uniform float maxFadeAltitudeKm;		// As a distance texcoord
	uniform float minSunlightAltitudeKm;	
	uniform uint cycled_index;
	uniform float overlayAlpha;

	uniform vec2 minimumPixelSize;
	uniform float sunBrightness;
	uniform float glowBrightness;

	//uniform mat4 cubemapViews[6]; messes up alignment if not at the end
SIMUL_CONSTANT_BUFFER_END

struct LightingQueryResult
{
	vec4 pos;
	vec4 sunlight;		// we use vec4's here to avoid padding.
	vec4 moonlight;
	vec4 ambient;
	vec3 sunlight_in_space;
	int  valid;
};
#endif