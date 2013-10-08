#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL
STATIC const int SIMUL_MAX_CLOUD_RAYTRACE_STEPS=200;

struct LayerData
{
	vec2 noiseOffset;
	float layerFade;
	float layerDistance;

	float verticalShift;
	float pad11;
	float pad12;
	float pad13;
};

struct SmallLayerData
{
	vec2 noiseOffset;
	float layerFade;
	float layerDistance;
	float verticalShift;
};

SIMUL_CONSTANT_BUFFER(LayerConstants,8)
	uniform LayerData layers[SIMUL_MAX_CLOUD_RAYTRACE_STEPS];
	uniform int layerCount;
	uniform int A,B,C;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(CloudPerViewConstants,13)
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 viewPos;
	uniform float uuuu;
	uniform mat4 invViewProj;
	uniform mat4 shadowMatrix;		// Transform from texcoords xy to world viewplane XYZ
	uniform mat4 noiseMatrix;
	uniform vec3 depthToLinFadeDistParams;
	uniform float exposure;
	uniform vec2 tanHalfFov;
	uniform float CloudPerViewConstantsPad1,CloudPerViewConstantsPad2;
	uniform float nearZ;
	uniform float farZ;
	uniform float extentZMetres;
	uniform float startZMetres;
	uniform float shadowRange;
	uniform int shadowTextureSize;
	uniform float depthMix;
	uniform float CloudPerViewConstantsPad3;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(CloudConstants,9)
	uniform vec3 inverseScales;
	uniform int abcde;
	uniform vec3 ambientColour;
	uniform float cloud_interp;
	uniform vec3 fractalScale;
	uniform float cloudEccentricity;
	uniform vec4 lightResponse;
	uniform vec3 directionToSun;
	uniform float earthshadowMultiplier;
	uniform vec3 cornerPos;
	uniform float hazeEccentricity;
	uniform vec4 lightningMultipliers;
	uniform vec4 lightningColour;
	uniform vec3 lightningSourcePos;
	uniform float rain;
	uniform vec3 sunlightColour1;
	uniform float fractalRepeatLength;
	uniform vec3 sunlightColour2;
	uniform float maxAltitudeMetres;
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
	uniform vec3 directionToMoon;
	uniform float baseNoiseFactor;
SIMUL_CONSTANT_BUFFER_END

#ifdef __cplusplus
//! A struct containing a pointer or id for the cloud shadow texture, along with 
//! information on how to project it.
uniform_buffer CloudShadowStruct 
{
	void *texture;			// texture, or SRV for DX11
	void *godraysTexture;	// texture, or SRV for DX11, represents accumulated illumination at a given angle and distance.
	mat4 shadowMatrix;
	mat4 simpleOffsetMatrix;
	float extentZMetres;
	float startZMetres;
	float shadowRange;
};
#endif
#endif