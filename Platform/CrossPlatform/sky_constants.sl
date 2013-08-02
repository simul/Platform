#ifndef SKY_CONSTANTS_SL
#define SKY_CONSTANTS_SL

uniform_buffer SkyConstants SIMUL_BUFFER_REGISTER(10)
{
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
	float maxFadeDistanceKm;
};

#endif