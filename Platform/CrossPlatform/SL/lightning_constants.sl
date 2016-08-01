//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightningConstants,10)
	uniform vec3 lightningColour;
	uniform float branchAngleRadians;

	uniform vec3 startPos;
	uniform uint num_octaves;

	uniform vec3 endPos;
	uniform float strikeThicknessMetres;

	uniform float roughness;
	uniform float motion;
	uniform uint numLevels;
	uniform uint numBranches;

	uniform float branchLengthMetres;
	uniform uint branchInterval;
	uniform float phaseTime;
	uniform int randomSeed;

	uniform float brightness;
	uniform float progress;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(LightningPerViewConstants,8)
	uniform mat4 worldViewProj;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec4 fullResToLowResTransformXYWH;
	uniform vec2 viewportPixels;
	uniform vec2 _line_width;

	uniform vec2 tanHalfFov;
	uniform float brightnessToUnity;
	uniform float minPixelWidth;

	uniform vec3 viewPosition;
	uniform float maxFadeDistance;
SIMUL_CONSTANT_BUFFER_END

struct LightningVertex
{
    vec4 position;
	vec2 texCoords;				// x= width in pixels
	float progress;
};

#ifndef __cplusplus
struct LightningVertexInput
{
    vec3 position		: SV_POSITION;
    vec2 texCoords		: TEXCOORD0;
	float progress		: TEXCOORD1;
};
struct LightningVertexOutput
{
    vec4 position			: SV_POSITION;
    float brightness		: TEXCOORD0;
    float thicknessMetres	: TEXCOORD1;
	float depth				: TEXCOORD2;
	float endpoint			: TEXCOORD3;
	float progress			: TEXCOORD4;
	vec3 view				: TEXCOORD5;
};
#endif

#endif