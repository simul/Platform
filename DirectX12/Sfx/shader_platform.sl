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
#ifndef BOTTOM_UP_TEXTURE_COORDINATES_DEFINED
	vec2 BottomUpTextureCoordinates(vec2 texc)
	{
		return vec2(texc.x,1.0-texc.y);
	}
	vec4 BottomUpTextureCoordinates4(vec4 texc)
	{
		return vec4(texc.x,1.0-texc.y,texc.z,1.0-texc.w);
	}
	#define BOTTOM_UP_TEXTURE_COORDINATES_DEFINED 1
#endif
#endif

#endif
