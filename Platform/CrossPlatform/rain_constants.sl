#ifndef RAIN_CONSTANTS_SL
#define RAIN_CONSTANTS_SL

uniform_buffer RainConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightColour;
	uniform vec3 lightDir;
	uniform float snowSize;
	uniform float offset;
	uniform float intensity;
	uniform float flurry,flurryRate;
	uniform float phase;
};

uniform_buffer RainPerViewConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform mat4 invViewProj;
	uniform vec3 viewPos;
	uniform float filld;
	uniform vec2 tanHalfFov;
	uniform float fill1,fill2;
	uniform float nearZ;
	uniform float farZ;
};
uniform_buffer RainOsdConstants SIMUL_BUFFER_REGISTER(9)
{
	uniform vec4 rect;
};
#endif