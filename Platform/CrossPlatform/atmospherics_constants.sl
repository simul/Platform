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
};

uniform_buffer AtmosphericsPerViewConstants R12
{
	uniform mat4 invViewProj;
	uniform mat4 shadowMatrix;
	uniform vec3 viewPosition;
	uniform float pad9;
	uniform vec2 tanHalfFov;
	uniform float fill1,fill2;
	uniform float nearZ;
	uniform float farZ;
};
#endif