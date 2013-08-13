#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL
STATIC const int MAX_INSTANCES=400;

struct LayerData
{
	vec2 noiseOffset;
	float pad1;
	float pad2;
	float noiseScale;
	float pad5;
	float pad6;
	float pad7;
	float layerFade;
	float pad8;
	float pad9;
	float pad10;
	float layerDistance;
	float verticalShift;
	float pad11;
	float pad12;
};

uniform_buffer LayerConstants R8
{
	uniform LayerData layers[MAX_INSTANCES];
	uniform int layerCount;
	uniform int A,B,C;
};

uniform_buffer CloudPerViewConstants R13
{
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 viewPos;
	uniform float uuuu;
	uniform mat4 invViewProj;
	uniform mat4 shadowMatrix;		// Transform from texcoords xy to world viewplane XYZ
	uniform mat4 noiseMatrix;
	uniform float nearZ;
	uniform float farZ;
	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float extentZMetres;
	uniform float startZMetres;
	uniform float shadowRange;
};
uniform_buffer CloudConstants R9
{
	uniform vec3 inverseScales;
	uniform int abcde;
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
	uniform vec3 cloudIrRadiance;
	uniform float x5;
};

#ifdef __cplusplus
//! A struct containing a pointer or id for the cloud shadow texture, along with 
//! information on how to project it.
uniform_buffer CloudShadowStruct 
{
	void *texture;	// texture, or SRV for DX11
	mat4 shadowMatrix;
	float extentZMetres;
	float startZMetres;
	float shadowRange;
};
#endif
#endif