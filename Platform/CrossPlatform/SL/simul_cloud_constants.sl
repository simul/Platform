#ifndef CLOUD_CONSTANTS_SL
#define CLOUD_CONSTANTS_SL

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
	uniform LayerData layers[255];
	uniform int layerCount;
	uniform int thisLayerIndex;
	uniform int B;
	uniform int C;
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

	uniform vec3 sunlightColour1;
	uniform float fractalRepeatLength;

	uniform vec3 sunlightColour2;
	uniform float maxAltitudeMetres;

	uniform vec2 screenCoordOffset;
	uniform vec2 rainTangent;

	uniform vec3 mieRayleighRatio;
	uniform float alphaSharpness;

	uniform float rain;
	uniform float maxFadeDistanceMetres;
	uniform float noise3DPersistence;
	uniform float pad1365124633463734;

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
	uniform vec4 localCloudPivot;    
	uniform vec4 localCloudInvScale; 
SIMUL_CONSTANT_BUFFER_END
	

SIMUL_CONSTANT_BUFFER(CloudLightpassConstants,10)
	uniform vec3 sourcePosMetres;
	uniform float sourceRadiusMetres;
	uniform vec3 spectralFluxOver1e6;			// Units of watts per nm
	uniform float maxCosine;
	uniform float irradianceThreshold;
	uniform float maxRadiusMetres;
SIMUL_CONSTANT_BUFFER_END

//! The result struct for a point or volume query.
struct VolumeQueryResult
{
	vec3 pos_m;
	int valid;
	float density;
	float direct_light;
	float indirect_light;
	float ambient_light;
};
//! The result struct for a line query.
struct LineQueryResult
{
	vec3 pos1_m;
	int valid;
	vec3 pos2_m;
	float density;
	float visibility;
	float optical_thickness_metres;
	float first_contact_metres;
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