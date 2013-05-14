#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../CrossPlatform/CppSl.hs"

#define fract frac
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture2D(tex,texc) tex.Sample(samplerState,texc)
#define texture3D(tex,texc) tex.Sample(samplerState3d,texc)
#define texture3D2(tex,texc) tex.Sample(samplerState3d2,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define uniform
#define uniform_buffer ALIGN_16 cbuffer
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D

#ifndef __cplusplus
	#define R0 : register(b0)
	#define R1 : register(b1)
	#define R2 : register(b2)
	#define R3 : register(b3)
	#define R4 : register(b4)
	#define R5 : register(b5)
	#define R8 : register(b8)
	#define R9 : register(b9)
	#define R10 : register(b10)
	#define R11 : register(b11)
	#define R12 : register(b12)
	#define vec1 float1
	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define mat4 float4x4
#endif

#ifdef __cplusplus
	#define cbuffer struct
	#define R0
	#define R1
	#define R2
	#define R3
	#define R4
	#define R5
	#define R6
	#define R7
	#define R8
	#define R9
#endif

#endif