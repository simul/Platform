#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/CppSl.hs"
// These definitions translate the HLSL terms cbuffer and R0 for GLSL or C++

#define uniform_buffer layout(std140) uniform
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
#define R10
#define R11
#define R12
#define R13

#ifndef __cplusplus
	#include "saturate.glsl"
	#define lerp mix
	#define texture_clamp_mirror(tex,texc) texture(tex,texc)
	#define sampleLod(tex,sampler,texc,lod) textureLod(tex,texc,lod);
	#define texture3D texture
	#define texture2D texture
	#define Y(texel) texel.y
	#define STATIC
#else
	#define STATIC static
#endif

#ifdef __cplusplus
	// To C++, samplers are just GLints.
	typedef int sampler1D;
	typedef int sampler2D;
	typedef int sampler3D;

	// C++ sees a layout as a struct, and doesn't care about uniforms
	#define layout(std140) struct
	#define uniform

#endif
#endif