#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../CrossPlatform/SL/CppSl.hs"

#ifndef __cplusplus
#define texture_clamp_mirror(tex,texc) tex.Sample(samplerStateClampMirror,texc)
#define texture_clamp(tex,texc) tex.Sample(clampSamplerState,texc)
#define texture_wrap_clamp(tex,texc) tex.Sample(wrapClampSamplerState,texc)
#define texture_wrap_mirror(tex,texc) tex.Sample(wrapMirrorSamplerState,texc)
#define texture_wrap_mirror_lod(tex,texc,lod) tex.SampleLevel(wrapMirrorSamplerState,texc,lod)
#define sample(tex,sampler,texc) tex.Sample(sampler,texc)
#define sampleLod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture2D(tex,texc) tex.Sample(samplerState,texc)
#define texture_wrap(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod)
#define texture_wrap_clamp_lod(tex,texc,lod) tex.SampleLevel(wrapClampSamplerState,texc,lod)
#define texture_cwc_lod(tex,texc,lod) tex.SampleLevel(cwcSamplerState,texc,lod)
#define texture_cmc_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_cmc_nearest_lod(tex,texc,lod) tex.SampleLevel(cmcNearestSamplerState,texc,lod)
#define texture_wmc_lod(tex,texc,lod) tex.SampleLevel(wmcSamplerState,texc,lod)
#define texture_wmc(tex,texc) tex.Sample(wmcSamplerState,texc)
#define texture_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)
#define texture_wrap_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearestWrap,texc,lod)
#define texture_clamp_mirror_lod(tex,texc,lod) tex.SampleLevel(samplerStateClampMirror,texc,lod)

#define texture_wwc(tex,texc) tex.Sample(wwcSamplerState,texc)
#define texture_nearest(tex,texc) tex.SampleLevel(samplerStateNearest,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture(tex,texc) tex.Sample(samplerState,texc)

#define texelFetch3d(tex,p,lod) tex.Load(int4(p,lod))
#define texelFetch2d(tex,p,lod) tex.Load(int3(p,lod))
#define imageStore(uav, pos, c) uav[pos]=c
#define image_load(tex,uint2pos) tex[uint2pos]
#endif

#define uniform
#define uniform_buffer ALIGN_16 cbuffer
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define STATIC static

#ifndef __cplusplus
	#define SIMUL_TEXTURE_REGISTER(tex_num) : register(t##tex_num)
	#define SIMUL_SAMPLER_REGISTER(samp_num) : register(s##samp_num)
	#define SIMUL_BUFFER_REGISTER(buff_num) : register(b##buff_num)
	#define SIMUL_RWTEXTURE_REGISTER(rwtex_num) : register(u##rwtex_num)
	#define SIMUL_STATE_REGISTER(snum) : register(s##snum)

	#define SIMUL_CONSTANT_BUFFER(name,buff_num) uniform_buffer name SIMUL_BUFFER_REGISTER(buff_num) {
	#define SIMUL_CONSTANT_BUFFER_END };

	#define SIMUL_TARGET_OUTPUT : SV_TARGET
	#define SIMUL_RENDERTARGET_OUTPUT(n) : SV_TARGET##n
	#define SIMUL_DEPTH_OUTPUT : SV_DEPTH

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
	struct positionColourVertexInput
	{
		vec3 position	: POSITION;
		vec4 colour		: TEXCOORD0;		
	};
	struct posTexVertexOutput
	{
		vec4 hPosition	: SV_POSITION;
		vec2 texCoords	: TEXCOORD0;		
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
	posTexVertexOutput VS_ScreenQuad(idOnly IN,vec4 rect)
	{
		posTexVertexOutput OUT;
		vec2 poss[4]=
		{
			{ 1.0, 0.0},
			{ 1.0, 1.0},
			{ 0.0, 0.0},
			{ 0.0, 1.0},
		};
		vec2 pos		=poss[IN.vertex_id];
		OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
		OUT.hPosition.z	=0.0; 
		OUT.texCoords	=pos;
		OUT.texCoords.y	=1.0-OUT.texCoords.y;
		return OUT;
	}
	
	vec4 texture_resolve(Texture2DMS<vec4> textureMS,vec2 texCoords)
	{
		uint2 dims;
		uint numberOfSamples;
		textureMS.GetDimensions(dims.x,dims.y,numberOfSamples);
		uint2 pos=uint2(vec2(dims)*texCoords);
		vec4 d=vec4(0,0,0,0);
		for(uint k=0;k<numberOfSamples;k++)
		{
			d+=textureMS.Load(pos,k);
		}
		d/=float(numberOfSamples);
		return d;
	}
#endif

#endif