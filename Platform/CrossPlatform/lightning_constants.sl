#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

uniform_buffer LightningConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightningColour;
};

uniform_buffer LightningPerViewConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform vec3 depthToLinFadeDistParams;
	uniform float xxxxxxx;
};

struct LightningVertex
{
    vec4 position;
	vec4 texCoords;
};

#ifndef __cplusplus
struct LightningVertexInput
{
    vec4 position		: POSITION;
    vec4 texCoords		: TEXCOORD0;
};
#endif

#endif