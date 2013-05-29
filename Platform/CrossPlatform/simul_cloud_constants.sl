#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL
STATIC const int MAX_INSTANCES=400;

struct LayerData
{
	vec2 noiseOffset;
	float pad1;
	float pad2;
	vec2 elevationRange;// unused
	float pad3;
	float pad4;
	float noiseScale;
	float layerFade;
	float layerDistance;
	float verticalShift;
};

uniform_buffer LayerConstants R8
{
	LayerData layers[MAX_INSTANCES];
};

uniform_buffer CloudInvViewProj R10
{
	uniform mat4 invViewProjtest;
};

uniform_buffer CloudConstants R9
{
	uniform mat4 worldViewProj;
	uniform mat4 invViewProj;
	uniform mat4 wrld;
	uniform mat4 noiseMatrix;
	uniform vec3 inverseScales;
	uniform int layerCount;
	uniform vec3 ambientColour;
	uniform float cloud_interp;
	uniform vec3 fractalScale;
	uniform float cloudEccentricity;
	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float earthshadowMultiplier;
	uniform vec3 cornerPos;
	uniform float hazeEccentricity;
	uniform vec4 lightningMultipliers;
	uniform vec4 lightningColour;
	uniform vec3 lightningSourcePos;
	uniform float rain;
	uniform vec3 sunlightColour1;
	uniform float x3;
	uniform vec3 sunlightColour2;
	uniform float x4;
	uniform vec2 screenCoordOffset;
	uniform vec2 y1;
	uniform vec3 mieRayleighRatio;
	uniform float alphaSharpness;
	uniform vec3 illuminationOrigin;
	uniform float maxFadeDistanceMetres;
	uniform vec3 illuminationScales	;
	uniform float noise3DPersistence;
	uniform vec3 crossSectionOffset	;
	uniform int noise3DOctaves;
	uniform vec3 noise3DTexcoordScale;
	uniform float z1;
	uniform vec2 tanHalfFov;
	uniform float fill1,fill2;
	uniform float nearZ;
	uniform float farZ;
};
#endif