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
#define texture(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture2D(tex,texc) tex.Sample(wrapSamplerState,texc)

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

	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define uchar4 float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define mix lerp
	#define fract frac

	#define layout(a)
	
#endif

#endif