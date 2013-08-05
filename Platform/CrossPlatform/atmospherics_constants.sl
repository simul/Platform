#ifndef ATMOSPHERICS_CONSTANTS_SL
#define ATMOSPHERICS_CONSTANTS_SL

uniform_buffer AtmosphericsUniforms R11
{
	uniform vec3 lightDir;
	uniform float pad1;
	uniform vec3 mieRayleighRatio;
	uniform float pad1a;
	uniform vec2 texelOffsets;
	uniform float pad2,pad3;
	uniform float hazeEccentricity;
	uniform float pad4,pad5,pad6;
	// X, Y and Z for the bottom-left corner of the cloud shadow texture.
	uniform vec3 cloudOrigin;
	uniform float pad7;
	uniform vec3 cloudScale;
	uniform float pad8;
	uniform float overcast;
	uniform float pad10,pad11,pad12;
	uniform float maxDistance;
	uniform float pad13,pad14,pad15;
	uniform float exposure;
	uniform float pad16,pad17,pad18;
	uniform float fogBaseAlt;
	uniform float fogScaleHeight;
	uniform float fogDensity;
	uniform float pad19;
	uniform vec3 fogColour;
	uniform float pad20;
    uniform vec3 infraredIntegrationFactors;
	uniform float pad21;
};

uniform_buffer AtmosphericsPerViewConstants R12
{
	uniform mat4 invViewProj;
	uniform mat4 invShadowMatrix;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 viewPosition;
	uniform float pad9;
	uniform vec2 tanHalfFov;
	uniform float fill1,fill2;
	uniform float nearZ;
	uniform float farZ;
	uniform float startZMetres;
	uniform float fill3;
	uniform float shadowRange;
	uniform float fill4,fill5,fill6;
	uniform float godrays_distances[100];
};
#endif
