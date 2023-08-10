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
	// This is a hack, dx11 effects do not recognise SetRenderTargetFormatState so 
	// we will pass a dummy SetGeometryShader(a), we should 
	#if PLATFORM_D3D11_SFX
		#define SetRenderTargetFormatState(a) //
	#else
		#define SetRenderTargetFormatState SetGeometryShader
	#endif
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET0
	#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET1
	vec2 BottomUpTextureCoordinates(vec2 texc)
	{
		return vec2(texc.x,1.0-texc.y);
	}
	vec4 BottomUpTextureCoordinates(vec4 texc)
	{
		return vec4(texc.x,1.0-texc.y,texc.z,1.0-texc.w);
	}
	#define BOTTOM_UP_TEXTURE_COORDINATES_DEFINED 1
#endif


#endif