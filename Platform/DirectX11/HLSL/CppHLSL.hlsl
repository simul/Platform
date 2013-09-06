#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../CrossPlatform/CppSl.hs"

#ifndef __cplusplus
#define texture_clamp_mirror(tex,texc) tex.Sample(samplerStateClampMirror,texc)
#define texture_clamp(tex,texc) tex.Sample(clampSamplerState,texc)
#define texture_wrap_clamp(tex,texc) tex.Sample(wrapClampSamplerState,texc)
#define texture_wrap_mirror(tex,texc) tex.Sample(wrapMirrorSamplerState,texc)
#define sampleLod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture2D(tex,texc) tex.Sample(samplerState,texc)
#define texture_wrap(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod)
#define texture_cwc_lod(tex,texc,lod) tex.SampleLevel(cwcSamplerState,texc,lod)
#define texture_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)
#define texture_wrap_nearest_lod(tex,texc,lod) tex.SampleLevel(wrapNearestSamplerState,texc,lod)
#define texture_clamp_mirror_lod(tex,texc,lod) tex.SampleLevel(samplerStateClampMirror,texc,lod)

#define texture_wwc(tex,texc) tex.Sample(wwcSamplerState,texc)
#define texture_nearest(tex,texc) tex.SampleLevel(samplerStateNearest,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture(tex,texc) tex.Sample(samplerState,texc)
#endif

#define uniform
#define uniform_buffer ALIGN_16 cbuffer
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define STATIC static

#ifndef __cplusplus
	#define SIMUL_TEXTURE_REGISTER(buff_num) : register(t##buff_num)
	#define SIMUL_SAMPLER_REGISTER(buff_num) : register(s##buff_num)
	#define SIMUL_BUFFER_REGISTER(buff_num) : register(b##buff_num)
	#define SIMUL_RWTEXTURE_REGISTER(tex_num) : register(u##tex_num)

	#define SIMUL_TARGET_OUTPUT : SV_TARGET
	#define SIMUL_DEPTH_OUTPUT : SV_DEPTH

	#define vec1 float1
	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define mix lerp
	#define fract frac

	#define Y(texel) texel.z
	
	SamplerState samplerStateClampMirror 
	{
		Filter = MIN_MAG_MIP_LINEAR;
		AddressU = Clamp;
		AddressV = Mirror;
	};

	struct idOnly
	{
		uint vertex_id			: SV_VertexID;
	};
	struct posTexVertexOutput
	{
		float4 hPosition	: SV_POSITION;
		float2 texCoords	: TEXCOORD0;		
	};
	posTexVertexOutput VS_SimpleFullscreen(idOnly IN)
	{
		posTexVertexOutput OUT;
		vec2 poss[4]=
		{
			{ 1.0,-1.0},
			{ 1.0, 1.0},
			{-1.0,-1.0},
			{-1.0, 1.0},
		};
		vec2 pos		=poss[IN.vertex_id];
		OUT.hPosition	=vec4(pos,0.0,1.0);
		OUT.hPosition.z	=0.0; 
		OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
		return OUT;
	}

#endif

#endif