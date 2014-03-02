#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

uniform_buffer LightningConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightColour;
};

uniform_buffer LightningPerViewConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj[2];
	uniform mat4 worldView[2];
	uniform mat4 invViewProj[2];
	uniform vec4 viewPos[2];
	uniform vec4 offset[2];
	uniform float nearZ;
	uniform float farZ;
	uniform vec3 depthToLinFadeDistParams;
	uniform float nearRainDistance;			// as a proportion of max fade distance
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec2 tanHalfFov;
	uniform vec2 dumm;
};

struct LightningVertex
{
    vec4 position;
	vec4 texCoords;
};

#endif