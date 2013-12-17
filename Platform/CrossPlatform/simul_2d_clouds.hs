#ifndef SIMUL_2D_CLOUDS_HS
#define SIMUL_2D_CLOUDS_HS

uniform_buffer Cloud2DConstants SIMUL_BUFFER_REGISTER(11)
{
	uniform vec4 viewportToTexRegionScaleBias;
	uniform mat4 worldViewProj;
	uniform vec3 origin;
	uniform float globalScale;

	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float cloudEccentricity;
	uniform vec3 ambientLight;
	uniform float extinction;
	uniform vec3 eyePosition;
	uniform float maxFadeDistanceMetres;
	uniform vec3 sunlight;
	uniform float cloudInterp;
	uniform vec3 mieRayleighRatio;
	uniform float hazeEccentricity;

	uniform float detailScale;
	uniform float planetRadius;
	uniform float fractalWavelength;
	uniform float fractalAmplitude;
	
	uniform vec2 tanHalfFov;
	uniform float nearZ;
	uniform float farZ;

	uniform vec3 depthToLinFadeDistParams;
	uniform float exposure;

	uniform float time;
	uniform float maxAltitudeMetres;
	uniform float offsetScale,maxCloudDistanceMetres;
	uniform vec3 moonlight;
	uniform float agagehrs;
};

uniform_buffer Detail2DConstants SIMUL_BUFFER_REGISTER(12)
{
	uniform float persistence;
	uniform int octaves;
	uniform float amplitude;
	uniform float density;						// Uniformly distributed thickness/cloudiness

	uniform vec3 lightDir2d;
	// for coverage
	uniform float coverageOctaves;

	uniform float coveragePersistence;
	uniform float humidity;
	uniform float diffusivity;
	uniform float noiseTextureScale;			// Scale from existing random texture to noise scale of coverage.
};

uniform_buffer CloudCrossSection2DConstants SIMUL_BUFFER_REGISTER(13)
{
	uniform vec4 rect;
};

#ifndef __cplusplus
#endif
#endif