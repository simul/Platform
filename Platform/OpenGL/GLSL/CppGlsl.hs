#ifndef GLSL_H
#define GLSL_H

#include "../../CrossPlatform/SL/CppSl.hs"
// These definitions translate the HLSL terms cbuffer and R0 for GLSL or C++
#define SIMUL_BUFFER_REGISTER(buff_num)
#define SIMUL_SAMPLER_REGISTER(buff_num)
#define SIMUL_BUFFER_REGISTER(buff_num)
#define SIMUL_TARGET_OUTPUT
#define	SIMUL_DEPTH_OUTPUT

#define GLSL

#ifndef __cplusplus
	#define uniform_buffer layout(std140) uniform
	#define SIMUL_CONSTANT_BUFFER(name,buff_num) uniform_buffer name {
	#define SIMUL_CONSTANT_BUFFER_END };
#include "saturate.glsl"
	#define lerp mix
	#define atan2 atan
	#define int2 ivec2
	#define int3 ivec3
	#define int4 ivec4
	#define uint2 uvec2
	#define uint3 uvec3
	#define uint4 uvec4
	#define texture_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap(tex,texc) texture(tex,texc)
	#define texture_clamp_mirror(tex,texc) texture(tex,texc)
	#define texture_wrap_clamp(tex,texc) texture(tex,texc)
	#define texture_wrap_mirror(tex,texc) texture(tex,texc) 
	#define sampleLod(tex,sampler,texc,lod) textureLod(tex,texc,lod)
	#define texture_wrap_lod(tex,texc,lod) textureLod(tex,texc,lod)
	#define texture_cwc_lod(tex,texc,lod) textureLod(tex,texc,lod)
	#define texture_clamp_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_nearest_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_clamp_mirror_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_cmc_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture_cmc_nearest_lod(tex,texc,lod) textureLod(tex,texc,lod) 
	#define texture3D texture
	#define texture2D texture 
	#define Texture3D sampler3D 
	#define Texture2D sampler2D 
	#define Texture1D sampler1D 
	#define Y(texel) texel.y
	#define STATIC
	vec4 mul(mat4 m,vec4 v)
	{
		return m*v;
	}
	vec2 mul(mat2 m,vec2 v)
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