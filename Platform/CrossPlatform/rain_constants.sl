#ifndef RAIN_CONSTANTS_SL
#define RAIN_CONSTANTS_SL

uniform_buffer RainConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform vec4 lightColour;
	uniform vec3 lightDir;
	uniform float snowSize;
	uniform vec3 meanFallVelocity;
	uniform float intensity;
	uniform float flurry;
	uniform float flurryRate;
	uniform float phase;
	uniform float timeStepSeconds;
};

uniform_buffer RainPerViewConstants SIMUL_BUFFER_REGISTER(8)
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

uniform_buffer RainOsdConstants SIMUL_BUFFER_REGISTER(9)
{
	uniform vec4 rect;
};

struct PrecipitationVertex
{
    vec3 position;	
	uint type;
	vec3 velocity;
	//float dummy;
};

#ifndef __cplusplus
struct PrecipitationVertexInput
{
    vec3 position	: POSITION;         //position of the particle
	uint type		: TYPE;             //particle type
	vec3 velocity	: VELOCITY;
	//float dummy		: DUMMY;
};
#endif

struct TransformedParticle
{
    vec4 position;
	float pointSize;
	float brightness;
	vec3 view;
	float fade;
};

#endif