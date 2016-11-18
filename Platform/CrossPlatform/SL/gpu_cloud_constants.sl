//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL
#define SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(GpuCloudConstants,8)
	uniform mat4 transformMatrix;
	uniform vec4 yRange;
	uniform vec3 noiseScale;
	uniform float sourceNoiseScaleKm;
	uniform vec2 extinctions;
	uniform float stepLength;
	uniform float cloudBaseKm;		
	uniform uint3 threadOffset;
	uniform int octaves;

	uniform float time;
	uniform float persistence;
	uniform float humidity;
	uniform float numWorleyOctaves;	// float for interpolation

	uniform float zPixel;
	uniform int numVolumes;
	uniform float baseLayer;
	uniform float transition;

	uniform float upperDensity;
	uniform float diffusivity;
	uniform float baseMixingRatio;
	uniform float maskThickness;

	uniform vec2 maskCentre;
	uniform float maskRadius;
	uniform float maskFeather;
	
	uniform vec3 cloudScalesKm;
	uniform float time_step;

	uniform vec3 lightDir;
	uniform float alpha;		// simulation constant;

	uniform float invBeta;
	uniform int wrap;
	uniform float viscousCoefficient;
	uniform float seaLevelTemperatureK;
	
	uniform vec3 cloudOriginKm;
	uniform float vorticityConfinement;

	uniform float worleyNoiseStrength;
	uniform float worleyNoiseScale;
	uniform float noisePeriod;
	uniform int numAffectors;
	
	uniform float localNoiseStrength;
	uniform float localCellNoiseStrength;
SIMUL_CONSTANT_BUFFER_END

#ifdef __cplusplus
enum CloudToolType
{
	NoTool=0
	,AddCloudTool=1
	,VortexTool=2
};
#else
#define CloudToolType int
#define NoTool (0)
#define AddCloudTool (1)
#define VortexTool (2)
#endif

struct CloudAffector
{
	vec3 pos;
	CloudToolType type;
	vec3 vel;
	float strength;
	vec3 uuuuu;
	float size;
};

struct CloudVolume_densityspace
{
	mat4 transformMatrix;	// transform density space to volume space.
	vec3 extents;
	int type;
	vec2 rake;
	vec2 cvdspad;
};
#endif
