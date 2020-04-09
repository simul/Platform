#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../Shaders/SL/CppSl.sl"

#ifndef __cplusplus
// Disable the warning "pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values..."
// because it is not helpful for optimized code.
#pragma warning( disable : 3571)
// Disable the "forcing loop to unroll" warning. Loop unrolling is a good thing.
#pragma warning( disable : 3557)
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
	#define mix lerp
	#define fract frac
	#define layout(a)
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET6883660##n
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET6883661##n
#endif

#endif