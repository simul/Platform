#ifndef CPP_HLSL
#define CPP_HLSL
#include "CppSl.sl"

#ifndef __cplusplus
// Disable the warning "pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values..."
// because it is not helpful for optimized code.
#pragma warning(disable : 3571)
// Disable the "forcing loop to unroll" warning. Loop unrolling is a good thing.
#pragma warning(disable : 3557)
// warning about using uint divides instead of int. Not always practical.
#pragma warning(disable : 3556)
// Because HLSL doesn't moan about seeing compute types in non-compute shaders, we can just:
#define IN_COMPUTE_SHADER
#define f32touint16 f32tof16

#endif


#define constant_buffer ALIGN_16 cbuffer

#ifndef __cplusplus
	#define char4 snorm float4
	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define uchar4 unorm float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define float16_t min16float
	#define f16vec2 min16float2
	#define f16vec3 min16float3
	#define f16vec4 min16float4
	#define f16mat2 min16float2x2
	#define f16mat3 min16float3x3
	#define f16mat4 min16float4x4
	#define mix lerp
	#define fract frac
	#define layout(a)
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET0
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET1
#endif

#endif
