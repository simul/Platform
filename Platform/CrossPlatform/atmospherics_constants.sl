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
	uniform vec3 cloudScale;
	uniform vec3 viewPosition;
	uniform float overcast;
	uniform float maxDistance;
	uniform float exposure;
};

uniform_buffer AtmosphericsUniforms2 R12
{
	uniform mat4 invViewProj;
	uniform vec2 tanHalfFov;
	uniform float fill1,fill2;
	uniform float nearZ;
	uniform float farZ;
};
#endif