#ifndef DX9_HLSL
#define DX9_HLSL
#define DX9
#include "../../CrossPlatform/CppSl.hs"
#ifndef __cplusplus
	#define SIMUL_TEXTURE_REGISTER(tex_num)
	#define SIMUL_SAMPLER_REGISTER(samp_num)
	#define SIMUL_BUFFER_REGISTER(buff_num)
	#define SIMUL_RWTEXTURE_REGISTER(rwtex_num)

	#define SIMUL_CONSTANT_BUFFER(name,buff_num)
	#define SIMUL_CONSTANT_BUFFER_END

	#define SIMUL_TARGET_OUTPUT : COLOR
	#define SIMUL_DEPTH_OUTPUT
#endif

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
	float4 generic_texture_lookup_lod(sampler2D tex,vec2 texc,int lod)
	{
		return tex2Dlod(tex,vec4(texc,0,(float)lod));
	}
	float4 generic_texture_lookup_lod(sampler3D tex,vec3 texc,int lod)
	{
		return tex3Dlod(tex,vec4(texc,(float)lod));
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
	#define texture_cmc_nearest_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)
	#define texture_clamp_mirror_lod(tex,texc,lod) generic_texture_lookup_lod(tex,texc,lod)

	#define texture_wwc(tex,texc) generic_texture_lookup(tex,texc)
	#define texture_nearest(tex,texc) generic_texture_lookup_lod(tex,texc)
	#define texture3Dpt(tex,texc) generic_texture_lookup(tex,texc)
	#define texture2Dpt(tex,texc) generic_texture_lookup(tex,texc)
	#define texture(tex,texc) generic_texture_lookup(tex,texc)


	#define uniform
	#define uniform_buffer struct
	#define STATIC static

	// Let's define some standard structs and shader functions to be used in .fx files:
#ifndef __cplusplus
	struct vertexInputPositionOnly
	{
		vec3 position		: POSITION;
	};

	struct vertexOutputPosTexc
	{
		vec4 hPosition		: POSITION;
		vec2 texCoords		: TEXCOORD0;
	};
	vertexOutputPosTexc VS_FullScreen(vertexInputPositionOnly IN)
	{
		vertexOutputPosTexc OUT;
		OUT.hPosition	=vec4(IN.position.xy,0.f,1.f);
		OUT.texCoords	=0.5*(IN.position.xy+vec2(1.0,1.0));
		return OUT;
	}
#endif

#endif