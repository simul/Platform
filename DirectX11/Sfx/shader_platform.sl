#ifndef SHADERPLATFORM_SL
#define SHADERPLATFORM_SL
#include "../../Shaders/SL/CppSl.sl"

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
// This is a hack, dx11 effects do not recognise SetRenderTargetFormatState so 
// we will pass a dummy SetGeometryShader(a), we should 
#if PLATFORM_D3D11_SFX
#define SetRenderTargetFormatState(a) //
#else
#define SetRenderTargetFormatState SetGeometryShader
#endif
#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET0
#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET1
#endif

#endif