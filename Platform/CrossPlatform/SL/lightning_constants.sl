#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

uniform_buffer LightningConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightningColour;
};

uniform_buffer LightningPerViewConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec2 viewportPixels;
	uniform vec2 _line_width;
	uniform vec4 viewportToTexRegionScaleBias;
};

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
#endif

#endif