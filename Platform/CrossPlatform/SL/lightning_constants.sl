#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightningConstants,10)
	uniform vec4 lightningColour;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(LightningPerViewConstants,8)
	uniform mat4 worldViewProj;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec2 viewportPixels;
	uniform vec2 _line_width;
	uniform vec2 tanHalfFov;
SIMUL_CONSTANT_BUFFER_END

struct LightningVertex
{
    vec4 position;
	vec4 texCoords;				// x= width in pixels
};

#ifndef __cplusplus
struct LightningVertexInput
{
    vec4 position		: POSITION;
    vec4 texCoords		: TEXCOORD0;
};
struct LightningVertexOutput
{
    vec4 position		: POSITION;
    vec4 texCoords		: TEXCOORD0;
	float depth			: TEXCOORD1;
};
#endif

#endif