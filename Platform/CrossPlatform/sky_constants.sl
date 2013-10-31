#ifndef SKY_CONSTANTS_SL
#define SKY_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SkyConstants,10)
	mat4 worldViewProj;
	mat4 proj;
	mat4 cubemapViews[6];
	vec4 eyePosition;
	vec4 lightDir;
	vec4 mieRayleighRatio;
	vec4 colour;
	float hazeEccentricity;
	float skyInterp;
	float altitudeTexCoord;
	float starBrightness;
	float radiusRadians;
	float overcast;
	float overcastBaseKm;
	float overcastRangeKm;
	
	uniform uint3 threadOffset;
	float maxFadeDistanceKm;
	float cloudShadowRange;		// As a distance texcoord
	float illumination_alt_texc;	// Due to earth shadow
	uint cycled_index;
	float a3;
SIMUL_CONSTANT_BUFFER_END

#endif