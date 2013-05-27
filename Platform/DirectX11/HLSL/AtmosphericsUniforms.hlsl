#include "CppHlsl.hlsl"
uniform_buffer AtmosphericsUniforms R11
{
	vec3 lightDir;
	float pad1;
	vec4 mieRayleighRatio;
	vec2 texelOffsets;
	float pad2,pad3;
	float hazeEccentricity;
	float pad4,pad5,pad6;
};

uniform_buffer AtmosphericsUniforms2 R12
{
	mat4 invViewProj;
	vec2 tanHalfFov;
	float fill1,fill2;
	float nearZ;
	float farZ;
};