#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../CrossPlatform/SL/CppSl.hs"

#ifndef __cplusplus
// Disable the warning "pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values..."
// because it is not helpful for optimized code.
#pragma warning( disable : 3571)
// Disable the "forcing loop to unroll" warning. Loop unrolling is a good thing.
#pragma warning( disable : 3557)
// Because HLSL doesn't moan about seeing compute types in non-compute shaders, we can just:
#define IN_COMPUTE_SHADER
#define shader
#define technique technique11
#define group fxgroup
#define f32touint16 f32tof16
#define texture_clamp_mirror(tex,texc) tex.Sample(cmcSamplerState,texc)
#define texture_clamp(tex,texc) tex.Sample(clampSamplerState,texc)
#define texture_wrap_clamp(tex,texc) tex.Sample(wrapClampSamplerState,texc)
#define texture_wrap_mirror(tex,texc) tex.Sample(wrapMirrorSamplerState,texc)
#define texture_wrap_mirror_lod(tex,texc,lod) tex.SampleLevel(wrapMirrorSamplerState,texc,lod)
#define sample(tex,sampler,texc) tex.Sample(sampler,texc)
#define sampleLod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define sample_lod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define texture(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture2D(tex,texc) tex.Sample(samplerState,texc)
#define texture_wrap(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod)
#define texture_wrap_clamp_lod(tex,texc,lod) tex.SampleLevel(wrapClampSamplerState,texc,lod)
#define texture_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)
#define texture_wrap_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearestWrap,texc,lod)
#define texture_clamp_mirror_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_cube(tex,texc) tex.Sample(cubeSamplerState,texc);

#define texture_wwc(tex,texc) tex.Sample(wwcSamplerState,texc)
#define texture_wwc_lod(tex,texc,lod) tex.SampleLevel(wwcSamplerState,texc,lod)
#define texture_nearest(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)

#define texture_clamp_mirror_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod)

#define texture_3d_cmc(tex,texc) tex.Sample(cmcSamplerState,texc)
#define texture_3d_nearest(tex,texc) tex.Sample(samplerStateNearest,texc) 
#define sample_3d(tex,sampler,texc) tex.Sample(sampler,texc) 
#define texture_3d_nearest_lod(tex,texc,lod) tex.SampleLevel(samplerStateNearest,texc,lod) 
#define texture_3d_clamp_lod(tex,texc,lod) tex.SampleLevel(clampSamplerState,texc,lod) 
#define texture_3d_wrap_lod(tex,texc,lod) tex.SampleLevel(wrapSamplerState,texc,lod)
#define texture_3d_clamp(tex,texc) tex.Sample(clampSamplerState,texc) 
#define texture_3d_wwc_lod(tex,texc,lod) tex.SampleLevel(wwcSamplerState,texc,lod) 
#define texture_3d_cwc_lod(tex,texc,lod) tex.SampleLevel(cwcSamplerState,texc,lod)
#define texture_3d_cmc_lod(tex,texc,lod) tex.SampleLevel(cmcSamplerState,texc,lod)
#define texture_3d_cmc_nearest_lod(tex,texc,lod) tex.SampleLevel(cmcNearestSamplerState,texc,lod)
#define texture_3d_wmc_lod(tex,texc,lod) tex.SampleLevel(wmcSamplerState,texc,lod)
#define texture_3d_wmc(tex,texc) tex.Sample(wmcSamplerState,texc)
#define sample_3d_lod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)

#define texelFetch3d(tex,p,lod) tex.Load(int4(p,lod))
#define texelFetch2d(tex,p,lod) tex.Load(int3(p,lod))
#define imageStore(uav, pos, c) uav[pos]=c
#define TEXTURE_LOAD(tex,uintpos) tex.Load(int3(uintpos,0))
#define TEXTURE_LOAD_3D(tex,uintpos) tex.Load(int4(uintpos,0))
#define TEXTURE_LOAD_MSAA(tex,uint2pos,sampl) tex.Load(uint2pos,sampl)

#define IMAGE_LOAD(tex,uintpos) tex[uintpos]
#define IMAGE_LOAD_3D(tex,uintpos) tex[uintpos]
#define	IMAGE_STORE(a,b,c) a[b]=c;
#define	IMAGE_STORE_3D(a,b,c) a[b]=c;
#endif

#define uniform
#define constant_buffer ALIGN_16 cbuffer
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

	#define SIMUL_CONSTANT_BUFFER(name,buff_num) constant_buffer name SIMUL_BUFFER_REGISTER(buff_num) {
	#define SIMUL_CONSTANT_BUFFER_END };

	#define SIMUL_TARGET_OUTPUT : SV_TARGET
	#define SIMUL_RENDERTARGET_OUTPUT(n) : SV_TARGET##n
	#define SIMUL_DEPTH_OUTPUT : SV_DEPTH

	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define uchar4 float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define mix lerp
	#define fract frac

	#define Y(texel) texel.z
	#define layout(a)
	#define CS_LAYOUT(u,v,w) [numthreads(u,v,w)]

	#define GET_DIMENSIONS_MSAA(tex,x,y,s) tex.GetDimensions(x,y,s)
	#define GET_DIMENSIONS(tex,x,y) tex.GetDimensions(x,y)
	#define GET_DIMENSIONS_3D(tex,x,y,z) tex.GetDimensions(x,y,z)
	#define GET_IMAGE_DIMENSIONS(tex,x,y) tex.GetDimensions(x,y)
	#define GET_IMAGE_DIMENSIONS_3D(tex,x,y,z) tex.GetDimensions(x,y,z)
	#define RW_TEXTURE3D_FLOAT4 RWTexture3D<float4>
	#define RW_TEXTURE3D_FLOAT RWTexture3D<float>
	#define RW_TEXTURE2D_FLOAT4 RWTexture2D<float4>
	#define TEXTURE2DMS_FLOAT4 Texture2DMS<float4>
	#define TEXTURE2D_UINT Texture2D<uint>
	#define TEXTURE2D_UINT4 Texture2D<uint4>
	#define TEXTURE2DMS_FLOAT4 Texture2DMS<float4>

	struct idOnly
	{
		uint vertex_id			: SV_VertexID;
	};

	
#endif

#endif