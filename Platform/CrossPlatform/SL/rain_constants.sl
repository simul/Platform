#ifndef RAIN_CONSTANTS_SL
#define RAIN_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(RainConstants,10)
	uniform vec4 lightColour;
	uniform vec3 lightDir;
	uniform float snowSize;
	uniform vec3 meanFallVelocity;
	uniform float intensity;
	uniform vec3 viewPositionOffset;
	uniform float flurry;
	uniform float flurryRate;
	uniform float phase;
	uniform float timeStepSeconds;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(MoisturePerViewConstants,9)
	uniform mat4 invViewProj;
	uniform vec4 depthViewport;		// xy = pos, zw = size
	uniform vec4 depthToLinFadeDist;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(RainPerViewConstants,8)
	uniform mat4 worldViewProj[2];
	uniform mat4 worldView[2];
	uniform mat4 invViewProj_2[2];
	uniform vec4 viewPos[2];
	uniform vec4 offset[2];
	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec2 tanHalfFov;
	uniform vec2 nearRainDistance;// as a proportion of max fade distance
	uniform float nearZ;
	uniform float farZ;
	uniform float qega;
	uniform float srhshrhrs;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(RainOsdConstants,9)
	uniform vec4 rect;
SIMUL_CONSTANT_BUFFER_END

struct PrecipitationVertex
{
    vec3 position;	
	uint type;
	vec3 velocity;
};
struct SplashVertex
{
    vec3 position;
	vec3 normal;
	float strength;
};

#ifndef __cplusplus
struct PrecipitationVertexInput
{
    vec3 position	: POSITION;         //position of the particle
	uint type		: TYPE;             //particle type
	vec3 velocity	: VELOCITY;
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