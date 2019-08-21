//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef WATER_CONSTANTS_SL
#define WATER_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(cbShading,1)

	// The strength, direction and color of sun streak
	uniform vec3		g_SunDir;
	uniform float		g_Shineness;
	uniform vec3		g_SunColor;
	uniform float		g_ShoreExtent;

	// Perlin noise for distant wave crest
	uniform float		g_PerlinSize;
	uniform vec3		g_PerlinAmplitude;
	uniform float		g_unityRender;
	uniform vec3		g_PerlinOctave;
	uniform float		g_oceanHeight;
	uniform float		g_vrRightEye;
	uniform vec2		g_viewportPixelScale;

	uniform vec3		g_PerlinGradient;
	uniform bool		g_enableFoam;

	// Constants for calculating texcoord from position
	uniform float		g_UVScale;
	uniform float		g_UVOffset;
	uniform float		g_WorldScale;
	uniform float		g_SunRadius;

	//Screen scale
	uniform vec2		g_DepthScale;
	uniform vec2		g_screenScale;

	//Depth parameters for depth calculations
	uniform vec4		g_DepthToLinFadeParams;
	uniform vec4		g_HalfTan;

	uniform vec3		g_ShoreDepthTextureLocation;
	uniform float		g_ShoreWidth;
SIMUL_CONSTANT_BUFFER_END

// Per draw call constants
SIMUL_CONSTANT_BUFFER(cbChangePerCall,2)
	// Transform matrices
	uniform mat4		g_matLocal;
	uniform mat4		g_matWorldViewProj;
	uniform mat4		g_matInvViewProj;
	uniform mat4		g_matWorld;

	// Misc per draw call constants
	uniform vec2		g_UVBase;
	uniform vec2		g_PerlinMovement;

	uniform vec3		g_LocalEye;
	uniform float		g_windDirection;

	uniform vec3		g_boundedDimension;
	uniform float		g_windDependency;

	uniform vec3		g_boundedLocation;
	uniform float		g_boundedRotation;

	uniform vec3		g_absorption;
	uniform uint		g_totalVertices;

	uniform vec3		g_scattering;
	uniform float		g_amplitude;

	uniform float		g_minQuadSize;
	uniform uint		g_verticiesPerLayer;
	uniform uint		g_layerDensity;
	uniform uint		g_noOfLayers;

	uniform uint2		g_boundedDensity;
	uniform float		g_profileUVScale;
	uniform float		g_foamStrength;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(OsdConstants,3)
	uniform vec2		bufferGrid;
	uniform float		showMultiplier;
	uniform float		agaher;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbWaterProbe, 4)
	uniform float		g_probeUVScale;
	uniform float		g_probeUVOffset;
	uniform float		g_surfaceHeight;
	uniform float		g_probeWaveAmplitude;

	uniform vec2		g_center;
	uniform float		g_probeWindDirection;
	uniform float		g_probeWindDependancy;

	uniform float		g_probeProfileUVScale;
	uniform float		g_waveGridScale;
	uniform bool		g_enableWaveGrid;
	uniform float		cvbhdxiobacyi;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbWaterFoam, 5)
	uniform float		g_foamHeight;
	uniform float		g_foamChurn;
	uniform vec2		hvyidbs;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbProfileBuffers, 6)
	uniform float		g_zetaMax;
	uniform float		g_zetaMin;
	uniform float		g_period;
	uniform float		g_integrationSteps;

	uniform float		g_time;
	uniform float		g_windSpeed;
	uniform int			g_totalWaveNumbers;
	uniform float		g_dt;

	uniform uint2		g_waveGridBound;
	uniform int			g_waveGroup;
	uniform float		gnfudivn;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbMeshObjectBuffers, 10)
	uniform float		g_buoyancyUVScale;
	uniform float		g_buoyancyUVOffset;
	uniform float		g_buoyancySurfaceHeight;
	uniform float		g_buoyancyWaveAmplitude;

	uniform vec2		g_buoyancyCenter;
	uniform float		g_buoyancyWindDirection;
	uniform float		g_buoyancyWindDependancy;

	uniform float		g_buoyancyProfileUVScale;
	uniform float		g_buoyancyWaveGridScale;
	uniform bool		g_buoyancyEnableWaveGrid;
	uniform float		gnuhjfidobn;

	uniform mat4		g_buoyancyMat;

	uniform vec3		g_location;
	uniform int			g_ID;

	uniform vec4		g_rotation;

	uniform vec3		g_scale;
	uniform float		vufiepadnu;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbContourBuffer, 8)
	uniform vec2		g_offsets;
	uniform float		g_waterHeight;
	uniform float		jnvuid9fsp;
	SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbFlowRayBuffer, 9)
	uniform float		g_stepSize;
	uniform float		g_maxDepth;
	uniform float		g_baseDirection;
	uniform float		g_baseWaveSpeed;
SIMUL_CONSTANT_BUFFER_END
#endif