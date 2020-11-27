//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SOLID_CONSTANTS_SL
#define SOLID_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SolidConstants,13)
	vec4 depthToLinFadeDistParams;
	vec4 fullResToLowResTransformXYWH;

	int lightCount;
	int reverseDepth;
	vec2 _pad0;

	vec4 diffuseOutputScalar;
	vec2 diffuseTexCoordsScalar_R;
	vec2 diffuseTexCoordsScalar_G;
	vec2 diffuseTexCoordsScalar_B;
	vec2 diffuseTexCoordsScalar_A;

	vec4 normalOutputScalar;
	vec2 normalTexCoordsScalar_R;
	vec2 normalTexCoordsScalar_G;
	vec2 normalTexCoordsScalar_B;
	vec2 normalTexCoordsScalar_A;

	vec4 combinedOutputScalarRoughMetalOcclusion;
	vec2 combinedTexCoordsScalar_R;
	vec2 combinedTexCoordsScalar_G;
	vec2 combinedTexCoordsScalar_B;
	vec2 combinedTexCoordsScalar_A;

	vec4 emissiveOutputScalar;
	vec2 emissiveTexCoordsScalar_R;
	vec2 emissiveTexCoordsScalar_G;
	vec2 emissiveTexCoordsScalar_B;
	vec2 emissiveTexCoordsScalar_A;

	vec3 u_SpecularColour;
	float _pad;

	float u_DiffuseTexCoordIndex;
	float u_NormalTexCoordIndex;
	float u_CombinedTexCoordIndex;
	float u_EmissiveTexCoordIndex;
SIMUL_CONSTANT_BUFFER_END

struct Light
{
	mat4 lightSpaceTransform;
	vec4 colour;

	vec3 position;
	float power;

	vec3 direction;
	float is_point;

	float is_spot;
	float radius;
	vec2 pad_light1;
};

#endif
