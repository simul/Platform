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
	uniform vec3 depthToLinFadeDistParams;
	uniform float startZMetres;
	uniform float shadowRange;
	uniform int godraysSteps;
	uniform float godraysDistanceStart;
	uniform float godraysDistanceStep;
};

/*
maybe use  D3D11_RESOURCE_MISC_BUFFER_STRUCTURED for godrays_distances

On Orbis See initAsRegularBuffer 

StructuredBuffer<unsigned int> indexBufferRO;
RWStructuredBuffer<unsigned int> indexBufferRW;
Orbis
RegularBuffer<unsigned int> indexBufferRO : register( t0 );
RW_RegularBuffer<unsigned int> indexBufferRW : register( u0 ); */
#endif
