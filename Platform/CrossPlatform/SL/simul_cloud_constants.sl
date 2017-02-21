//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL

struct LayerData
{
	vec2 noiseOffset;
	float pad11;
	float pad12;
	float layerFade;
	float layerDistanceKm;
	float verticalShift;
	float pad13;
};

SIMUL_CONSTANT_BUFFER(CloudStaticConstants,12)
	uniform mat4 invViewProj[6];	// Cubemap matrices
SIMUL_CONSTANT_BUFFER_END


SIMUL_CONSTANT_BUFFER(CloudPerViewConstants,13)

	uniform uint4 targetRange[6];

	uniform mat4 shadowMatrix;		// Transform from texcoords xy to world viewplane XYZ

	uniform mat4 worldToScatteringVolumeMatrix;

	uniform vec4 depthToLinFadeDistParams;

	uniform vec3 scaleOfGridCoords;
	uniform int halfClipSize;			// Actually half the full clip size.

	uniform vec3 gridOriginPosKm;
	uniform int cubemapViewIndex;

	uniform vec3 viewPosKm;
	uniform float nearZ_deprecated;

	uniform float shadowRangeKm;
	uniform int shadowTextureSize;
	uniform float depthMix;
	uniform int cubemapTargetIndex;

	uniform uint3 amortizationOffset;
	uniform float exposure;
	
	uniform uint2 targetTextureSize;
	uniform uint2 edge;

	uniform uint3 amortizationScale;
	uniform float maxCloudDistanceKm;

	uniform uint4 cubemapFaceIndex[6];

	uniform vec3 scale;
	uniform float cloud_interp;

	uniform vec3 offset;
	uniform float azimuth;

	uniform int2 gridCentreTexel;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(CloudConstants,9)
	uniform vec3 inverseScalesKm;
	uniform float rainbowIntensity;

	uniform vec3 ambientColour;
	uniform float cloud_interpXXXXX;

	uniform vec3 fractalScale;
	uniform float cloudEccentricity;

	uniform vec4 lightResponse;

	uniform vec3 directionToSun;
	uniform float earthshadowMultiplier;

	uniform vec3 cornerPosKm;
	uniform float hazeEccentricity;

	uniform vec3 sunlightColour1;
	uniform float fractalRepeatLength;

	uniform vec3 sunlightColour2;
	uniform float fadeAltitudeRangeKm;

	uniform vec2 screenCoordOffset;
	uniform vec2 rainTangent;

	uniform vec3 mieRayleighRatio;
	uniform float alphaSharpness;

	uniform float rain;
	uniform float maxFadeDistanceKm;
	uniform float noise3DPersistence;
	uniform float minSunlightAltitudeKm;

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

	uniform float precipitationThreshold;
	uniform float worleyScale;
	uniform float worleyNoise;
	uniform int firstInput;

	uniform vec3 rainCentreKm;
	uniform float rainRadiusKm;

	uniform vec3 rainNoiseInvScale;
	uniform float rainVerticalTexcoord;

	uniform uint3 windowGrid;
	uniform float rainEdgeKm;
	uniform float precipitation;
SIMUL_CONSTANT_BUFFER_END
	

SIMUL_CONSTANT_BUFFER(CloudLightpassConstants,10)
	uniform vec3 sourcePosKm;
	uniform float sourceRadiusKm;
	uniform vec3 spectralFluxOver1e6;			// Units of watts per nm
	uniform float minCosine;
	uniform float irradianceThreshold;
	uniform float maxRadiusKm;
SIMUL_CONSTANT_BUFFER_END

//! The result struct for a point or volume query.
struct VolumeQueryResult
{
	vec3 pos_km;
	int valid;
	float density;
	float direct_light;
	float indirect_light;
	float ambient_light;
	float precipitation;
};
//! The result struct for a line query.
struct LineQueryResult
{
	vec3 pos1_km;
	int valid;
	vec3 pos2_km;
	float density;
	float visibility;
	float optical_thickness_km;
	float first_contact_km;
};
										  
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
	simul::crossplatform::Texture *rainMapTexture;			///< Texture represents where in the horizontal plane of the cloud rain can fall.
	mat4 shadowMatrix;					// Transform a position from shadow space to world space
	mat4 simpleOffsetMatrix;
	mat4 worldspaceToCloudspaceMatrix;
	float minAltKm;
	float maxAltKm;
	float shadowRangeKm;
	float rainbowIntensity;
	float godrays_strength;
	bool wrap;
	simul::crossplatform::SamplerState *samplerState;
	vec3 originKm;
	vec3 scaleKm;
};
#else
#endif
#endif