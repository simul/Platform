#ifndef OCEAN_CONSTANTS_SL
#define OCEAN_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(FftConstants,0)
	uint thread_count;
	uint ostride;
	uint istride;
	uint pstride;
	float phase_base;
	float ahehsj,tdjtdjt,jttztjz;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbImmutable,0)
	uniform uint g_ActualDim;
	uniform uint g_InWidth;
	uniform uint g_OutWidth;
	uniform uint g_OutHeight;
	uniform uint g_DxAddressOffset;
	uniform uint g_DyAddressOffset;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbChangePerFrame,1)
	uniform float g_Time;
	uniform float g_ChoppyScale;
	uniform float g_GridLen;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(cbShading,2)
	// The color of bottomless water body
	uniform vec3		g_WaterbodyColor;

	// The strength, direction and color of sun streak
	uniform float		g_ShinenessX;
	uniform vec3		g_SunDirX;
	uniform float		xxxxxxxxxxx;
	uniform vec3		g_SunColorX;
	uniform float		xxxxxxxxxxxx;
	
	// The parameter is used for fixing an artifact
	uniform vec3		g_BendParam;

	// Perlin noise for distant wave crest
	uniform float		g_PerlinSize;
	uniform vec3		g_PerlinAmplitude;
	uniform float		xxxxxxxxxxxxxxx;
	uniform vec3		g_PerlinOctave;
	uniform float		xxxxxxxxxxxxxxxx;
	uniform vec3		g_PerlinGradient;
	uniform float		xxxxxxxxxxxxxxxxx;

	// Constants for calculating texcoord from position
	uniform float		g_TexelLength_x2;
	uniform float		g_UVScale;
	uniform float		g_UVOffset;
	uniform float		xxxxxxxxxxxxxxxxxx;
SIMUL_CONSTANT_BUFFER_END

// Per draw call constants
SIMUL_CONSTANT_BUFFER(cbChangePerCall,4)
	// Transform matrices
	uniform mat4	g_matLocal;
	uniform mat4	g_matWorldViewProj;
	uniform mat4	g_matWorld;

	// Misc per draw call constants
	uniform vec2		g_UVBase;
	uniform vec2		g_PerlinMovement;
	uniform vec3		g_LocalEye;

	// Atmospherics
	uniform float		hazeEccentricity;
	uniform vec4		mieRayleighRatio;
	uniform vec3		lightDir;
	uniform float		srjdtkfylu;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(OsdConstants,5)
	uniform vec4 rect;
	uniform vec2 bufferGrid;
	uniform float showMultiplier;
	uniform float agaher,reajst;
SIMUL_CONSTANT_BUFFER_END

#endif