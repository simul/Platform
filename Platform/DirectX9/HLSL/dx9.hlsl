#ifndef DX9_HLSL
#define DX9_HLSL
#include "../../CrossPlatform/CppSl.hs"

	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define mix lerp
	#define fract frac

	#define Texture1D sampler1D
	#define Texture2D sampler2D
	#define Texture3D sampler3D

	float4 generic_texture_lookup(sampler2D tex,vec2 texc)
	{
		return tex2D(tex,texc);
	}
	float4 generic_texture_lookup(sampler3D tex,vec3 texc)
	{
		return tex3D(tex,texc);
	}
	#define texture_clamp_mirror(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_clamp(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_wrap_clamp(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_wrap_mirror(tex,texc) generic_texture_lookup(tex,texc)
	#define sampleLod(tex,sampler,texc,lod) generic_texture_lookupLevel(sampler,texc,lod)
	#define texture(tex,texc) generic_texture_lookup(tex,texc)
	#define texture2D(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_wrap(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_wrap_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_clamp_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_wrap_clamp_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_cwc_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_cmc_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_nearest_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_clamp_mirror_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)

	#define texture_wwc(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_nearest(tex,texc) generic_texture_lookup_lod(tex,texc)
	#define texture3Dpt(tex,texc) generic_texture_lookup(tex,texc)
	#define texture2Dpt(tex,texc) generic_texture_lookup(tex,texc)
	#define texture(tex,texc) generic_texture_lookup(tex,texc)


	#define uniform
	#define uniform_buffer struct
	#define STATIC static

	#define SIMUL_CONSTANT_BUFFER(name,buff_num)
	#define SIMUL_CONSTANT_BUFFER_END
#endif