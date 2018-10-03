//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef WATER_CONSTANTS_SL
#define WATER_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(FftConstants,11)
	uint thread_count;
	uint ostride;
	uint istride;
	uint pstride;
	float phase_base;
	float ahehsj;
	float tdjtdjt;
	float jttztjz;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbImmutable,1)
	uniform uint g_ActualDim;
	uniform uint g_InWidth;
	uniform uint g_OutWidth;
	uniform uint g_OutHeight;
	uniform uint g_DxAddressOffset;
	uniform uint g_DyAddressOffset;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbChangePerFrame,2)
	uniform float g_Time;
	uniform float g_ChoppyScale;
	uniform vec2 bnvyduf9sabyv;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbShading,3)

	// The strength, direction and color of sun streak
	uniform vec3		g_SunDir;
	uniform float		g_Shineness;
	uniform vec3		g_SunColor;
	uniform float		g_ShoreExtent;

	// Perlin noise for distant wave crest
	uniform vec3		fnbhjdiosabh;
	uniform float		g_PerlinSize;
	uniform vec3		g_PerlinAmplitude;
	uniform float		g_unityRender;
	uniform vec3		g_PerlinOctave;
	uniform float		g_oceanHeight;

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
SIMUL_CONSTANT_BUFFER(cbChangePerCall,4)
	// Transform matrices
	uniform mat4	g_matLocal;
	uniform mat4	g_matWorldViewProj;
	uniform mat4	g_matInvViewProj;
	uniform mat4	g_matWorld;

	// Misc per draw call constants
	uniform vec2		g_UVBase;
	uniform vec2		g_PerlinMovement;
	uniform vec3		g_LocalEye;
	uniform float		g_windDirection3;

	uniform vec3		g_boundedDimension;
	uniform float		g_windDependency3;

	uniform vec3		g_boundedLocation;
	uniform float		g_boundedRotation;

	uniform vec3		g_absorption;
	uniform uint		g_totalVertices;
	uniform vec3		g_scattering;
	uniform float		g_amplitude2;

	uniform float		g_minQuadSize;
	uniform uint		g_verticiesPerLayer;
	uniform uint		g_layerDensity;
	uniform uint		g_noOfLayers;

	uniform vec2		g_boundedDensity;
	uniform float		g_profileUVScale;
	uniform float		g_foamStrength;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(OsdConstants,5)
	uniform vec2 bufferGrid;
	uniform float showMultiplier;
	uniform float agaher;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbHeightmap, 6)
	uniform vec2	g_windDir;
	uniform float	g_amplitude;
	uniform float	g_windSpeed;

	uniform float	g_windDependency;
	uniform uint	g_gridSize;
	uniform float	fgdsvcx;	
	uniform float	hgfdjkslgh;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbWaterProbe, 8)
	uniform float g_UVScale2;
	uniform float g_UVOffset2;
	uniform float g_surfaceHeight;
	uniform float g_probeWaveAmplitude;

	uniform vec2 g_center;
	uniform float g_probeWindDirection;
	uniform float g_probeWindDependancy;

	uniform float g_probeProfileUVScale;
	uniform vec3 cvbhdxiobacyi;
SIMUL_CONSTANT_BUFFER_END


SIMUL_CONSTANT_BUFFER(cbWaterFoam, 9)
	//Beaufort scaling
	uniform float		g_foamHeight;
	uniform float		g_foamChurn;
	uniform vec2		hvyidbs;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbProfileBuffers, 10)
	uniform float		g_zetaMax;
	uniform float		g_zetaMin;
	uniform float		g_period;
	uniform float		g_integrationSteps;

	uniform float		g_time;
	uniform float		g_windSpeed2;
	uniform int			g_totalWaveNumbers;
	uniform float		g_dt;

	uniform vec2		g_windDirection2;
	uniform float		g_windDependency2;
	uniform int			g_waveGroup;
SIMUL_CONSTANT_BUFFER_END
#endif