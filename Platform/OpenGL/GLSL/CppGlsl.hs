#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/CppSl.hs"
// These definitions translate the HLSL terms cbuffer and R0 for GLSL or C++
#define SIMUL_BUFFER_REGISTER(buff_num)
#define SIMUL_SAMPLER_REGISTER(buff_num)
#define SIMUL_BUFFER_REGISTER(buff_num)

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
#define R13
#define GLSL

#ifndef __cplusplus
	#define uniform_buffer layout(std140) uniform
	#include "saturate.glsl"
	#define lerp mix
	#define atan2 atan 
	#define texture_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap(tex,texc) texture(tex,texc)
	#define texture_clamp_mirror(tex,texc) texture(tex,texc)
	#define texture_wrap_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap_mirror(tex,texc) texture(tex,texc)
	#define sampleLod(tex,sampler,texc,lod) textureLod(tex,texc,lod)
	#define texture3D texture
	#define texture2D texture
	#define Y(texel) texel.y
	#define STATIC
vec4 mul(mat4 m,vec4 v)
{
	return m*v;
}
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