#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL
STATIC const int SIMUL_MAX_CLOUD_RAYTRACE_STEPS=255;

struct LayerData
{
	vec2 noiseOffset;
	float pad11;
	float pad12;
	float layerFade;
	float layerDistanceMetres;
	float verticalShift;
	float pad13;
};

SIMUL_CONSTANT_BUFFER(SingleLayerConstants,5)
	vec2 noiseOffset_;
	float layerFade_;
	float layerDistance_;
	float verticalShift_;
	float pad121;
	float pad122;
	float pad123;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(LayerConstants,8)
	uniform LayerData layers[SIMUL_MAX_CLOUD_RAYTRACE_STEPS];
	uniform int layerCount;
	uniform int thisLayerIndex,B,C;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(CloudPerViewConstants,13)
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec3 scaleOfGridCoords;
	uniform int halfClipSize;			// Actually half the full clip size.
	uniform vec3 viewPos;
	uniform uint layerIndex;
	uniform mat4 invViewProj;
	uniform mat4 shadowMatrix;		// Transform from texcoords xy to world viewplane XYZ
	uniform mat4 moistureToWorldSpaceMatrix;
	uniform mat4 clipPosToScatteringVolumeMatrix;
	uniform mat4 worldViewProj;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float maxCloudDistanceMetres;
	uniform float nearZ;
	uniform float farZ;
	uniform float extentZMetres;
	uniform float startZMetres;
	uniform float shadowRange;
	uniform int shadowTextureSize;
	uniform float depthMix;
	uniform float CloudPerViewConstantsPad3;
	uniform vec4 mixedResTransformXYWH;		// xy=offset, zw=multiple.
	uniform vec2 rainCentre;
	uniform float rainRadius;
	uniform float ageaeghj5;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(CloudConstants,9)
	uniform vec3 inverseScales;
	uniform float rainbowIntensity;
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
	uniform vec2 rainTangent;
	uniform vec3 mieRayleighRatio;
	uniform float alphaSharpness;
	uniform vec3 lightningOrigin;
	uniform float maxFadeDistanceMetres;
	uniform vec3 lightningInvScales;
	uniform float noise3DPersistence;
	uniform vec3 crossSectionOffset;
	uniform int noise3DOctaves;
	uniform vec3 noise3DTexcoordScale;
	uniform float rainEffect;
	uniform vec3 cloudIrRadiance1;
	uniform float yz;
	uniform vec3 cloudIrRadiance2;
	uniform float noise3DOctaveScale;
	uniform vec3 directionToMoon;
	uniform float baseNoiseFactor;
	uniform vec3 noise3DTexcoordOffset;
	uniform float dropletRadius;
	// RDE begin: added
	uniform vec4 infraredIntegrationFactorsUNUSED;
	// local cloud support
	uniform vec4 localCloudPivot;    
	uniform vec4 localCloudInvScale; 
	//uniform int3 cloudGrid;
	//uniform float aq0o84hq4hl;
SIMUL_CONSTANT_BUFFER_END
	
										  
#ifdef __cplusplus
//! A struct containing a pointer or id for the cloud shadow texture, along with 
//! information on how to project it.
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class SamplerState;
	}
}
struct CloudShadowStruct 
{
	simul::crossplatform::Texture *cloudTexture;			///< The cloud texture.
	simul::crossplatform::Texture *cloudShadowTexture;		///< Cloud shadow texture.
	simul::crossplatform::Texture *godraysTexture;			///< Texture represents accumulated illumination at a given angle and distance.
	simul::crossplatform::Texture *moistureTexture;			///< Texture represents optical thickness of moisture at a given horizontal angle and distance.
	simul::crossplatform::Texture *rainMapTexture;			///< Texture represents where in the horizontal plane of the cloud rain can fall.
	mat4 shadowMatrix;					// Transform a position from shadow space to world space
	mat4 worldToMoistureSpaceMatrix;	// Transform a position from world space to moisture space.
	mat4 simpleOffsetMatrix;
	mat4 worldspaceToCloudspaceMatrix;
	float extentZMetres;
	float startZMetres;
	float shadowRange;
	float rainbowIntensity;
	float godrays_strength;
	bool wrap;
	simul::crossplatform::SamplerState *samplerState;
};
#else
//SIMUL_CONSTANT_BUFFER(OnscreenRectangle,9)
	uniform vec4 rect;
//SIMUL_CONSTANT_BUFFER_END
#endif
#endif