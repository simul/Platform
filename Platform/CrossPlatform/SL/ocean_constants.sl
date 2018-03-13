//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef OCEAN_CONSTANTS_SL
#define OCEAN_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(FftConstants,0)
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
	uniform float xxxxxx;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbShading,3)

	// The strength, direction and color of sun streak
	uniform vec3		g_SunDir;
	uniform float		g_Shineness;
	uniform vec3		g_SunColor;
	uniform bool		g_PassNo;

	// Perlin noise for distant wave crest
	
	uniform vec3		g_PerlinAmplitude;
	uniform float		g_PerlinSize;
	uniform vec3		g_PerlinOctave;
	uniform float		g_boundedRotation;
	uniform vec3		g_PerlinGradient;

	// Constants for calculating texcoord from position
	uniform float		g_TexelLength_x2;
	uniform float		g_UVScale;
	uniform float		g_UVOffset;
	uniform float		g_WorldScale;
	uniform float		g_minQuadSize;
	uniform uint		g_verticiesPerLayer;
	uniform uint		g_layerDensity;
	uniform uint		g_totalVertices;
	uniform uint		g_noOfLayers;

	uniform vec3		g_boundedDimension;
	uniform bool		g_enableRefraction;
	uniform vec2		g_boundedDensity;
	uniform vec2		g_DepthScale;
	uniform vec3		g_boundedLocation;
	uniform float		g_waterHeight;

	uniform vec3		g_scattering;
	uniform float		g_salinity;
	uniform vec3		g_absorption;
	uniform float		g_temperature;

	//Depth parameters for depth calculations
	uniform vec4		g_DepthToLinFadeParams;
	uniform vec4		g_HalfTan;

	//Beaufort scaling
	uniform float		g_foamHeight;
	uniform float		g_foamChurn;

	//Screen scale
	uniform vec2		g_screenScale;
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
	uniform float		qjk345rstj;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(OsdConstants,5)
	uniform vec2 bufferGrid;
	uniform float showMultiplier;
	uniform float agaher;
	uniform float reajst;
	SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbHeightmap, 6)
	uniform vec2	g_windDir;
	uniform float	g_amplitude;
	uniform float	g_windSpeed;

	uniform float	g_windDependency;
	uniform uint	g_gridSize;
	uniform float	g_rand;	
	uniform float	hgfdjkslgh;
SIMUL_CONSTANT_BUFFER_END

#endif