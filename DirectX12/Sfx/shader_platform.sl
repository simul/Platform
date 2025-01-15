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


#endif

#define constant_buffer ALIGN_16 cbuffer

#ifndef __cplusplus
	#define layout(a)
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET0
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET1

	#ifndef BOTTOM_UP_TEXTURE_COORDINATES
	#define BOTTOM_UP_TEXTURE_COORDINATES 1
	#endif

	float roundNearest(float x)
	{
		float x_f = floor(x);
		float x_c = ceil(x);
		return x_c - x <= x - x_f ? x_c : x_f;
	}
	float2 roundNearest(float2 x) { return float2(roundNearest(x.x), roundNearest(x.y)); }
	float3 roundNearest(float3 x) { return float3(roundNearest(x.x), roundNearest(x.y), roundNearest(x.z)); }
	float4 roundNearest(float4 x) { return float4(roundNearest(x.x), roundNearest(x.y), roundNearest(x.z), roundNearest(x.w)); }

	float4x4 roundNearest(float4x4 x) { return float4x4(roundNearest(x[0]), roundNearest(x[1]), roundNearest(x[2]), roundNearest(x[3])); }
	float3x3 roundNearest(float3x3 x) { return float3x3(roundNearest(x[0]), roundNearest(x[1]), roundNearest(x[2])); }
	float2x2 roundNearest(float2x2 x) { return float2x2(roundNearest(x[0]), roundNearest(x[1])); }

#endif

#endif
