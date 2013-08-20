#ifndef ATMOSPHERICS_CONSTANTS_SL
#define ATMOSPHERICS_CONSTANTS_SL

uniform_buffer AtmosphericsUniforms SIMUL_BUFFER_REGISTER(11)
{
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
};

uniform_buffer AtmosphericsPerViewConstants SIMUL_BUFFER_REGISTER(12)
{
	uniform mat4 invViewProj;
	uniform mat4 invShadowMatrix;
	uniform mat4 shadowMatrix;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 viewPosition;
	uniform float pad9;
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float farZ;
	uniform float startZMetres;
	uniform float shadowRange;
	uniform int godraysSteps;
	uniform float fill4;
	uniform vec4 godrays_distances[200];	// We use vec4 instead of float, because DirectX debug reports an error - perhaps floats are packed as float4 for arrays.
};
#endif
