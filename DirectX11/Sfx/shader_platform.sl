#ifndef SHADERPLATFORM_SL
#define SHADERPLATFORM_SL
#include "CppSl.sl"

#pragma warning( disable : 4717)
#pragma warning( disable : 3550)

#ifndef __cplusplus
#define flat
// Disable the warning "pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values..."
// because it is not helpful for optimized code.
#pragma warning( disable : 3571)
// Disable the "forcing loop to unroll" warning. Loop unrolling is a good thing.
#pragma warning( disable : 3557)
// Disable more unhelpful warnings
#pragma warning(disable:3556,4717)
#pragma warning(disable : 3556)	// warning about using uint divides instead of int. Not always practical.
// Because HLSL doesn't moan about seeing compute types in non-compute shaders, we can just:
#define IN_COMPUTE_SHADER

#if !PLATFORM_D3D11_SFX
#define shader
#define technique technique11
#define group fxgroup
#endif
#define f32touint16 f32tof16


#endif
#if !PLATFORM_D3D11_SFX
#define SetTopology( a ) SetGeometryShader(NULL)
#endif

#define constant_buffer ALIGN_16 cbuffer
#if !PLATFORM_D3D11_SFX
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#endif

#ifndef __cplusplus
	#define layout(a)
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET0
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET1
	
	#ifndef BOTTOM_UP_TEXTURE_COORDINATES
	#define BOTTOM_UP_TEXTURE_COORDINATES 1
	#endif
#endif


#endif