#ifndef CPP_GLSL
#define CPP_GLSL

#include "CppSl.sl"

#ifndef __cplusplus

#ifndef BOTTOM_UP_TEXTURE_COORDINATES
#define BOTTOM_UP_TEXTURE_COORDINATES 1
#endif

#define USE_D3D_REF_MODE 0

// This should be passed to the compiler, do we use it? (gaussian.sfx)
#define SCAN_SMEM_SIZE 128
#define THREADS_PER_GROUP 128

float saturate(float value)	{return clamp(value, 0.0, 1.0);}
vec2 saturate(vec2 vvalue)	{return clamp(vvalue,vec2(0.0,0.0),vec2(1.0,1.0));}
vec3 saturate(vec3 vvalue)	{return clamp(vvalue,vec3(0.0,0.0,0.0),vec3(1.0,1.0,1.0));}
vec4 saturate(vec4 vvalue)	{return clamp(vvalue,vec4(0.0,0.0,0.0,0.0),vec4(1.0,1.0,1.0,1.0));}

vec4 mul(mat4 mat, vec4 vec){return mat * vec;}
vec3 mul(mat3 mat, vec3 vec){return mat * vec;}
vec2 mul(mat2 mat, vec2 vec){return mat * vec;}

vec4 mul(vec4 vec, mat4 mat){return vec * mat;}
vec3 mul(vec3 vec, mat3 mat){return vec * mat;}
vec2 mul(vec2 vec, mat2 mat){return vec * mat;}

uint reversebits(uint value){return bitfieldReverse(value);}

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define f16vec4 uvec4
#define uint64_t uvec2


// Functions to FORCE mod (or % operator) to return the correct type, for poorly-implemented GLSL compilers:
int typed_mod(int a,int b)
{
	return int(mod(a,b));
}
int2 typed_mod(int2 a,int2 b)
{
	return int2(mod(a,b));
}
int3 typed_mod(int3 a,int3 b)
{
	return int3(mod(a,b));
}
int4 typed_mod(int4 a,int4 b)
{
	return int4(mod(a,b));
}
uint typed_mod(uint a,uint b)
{
	return uint(mod(a,b));
}
uint2 typed_mod(uint2 a,uint2 b)
{
	return uint2(mod(a,b));
}
uint3 typed_mod(uint3 a,uint3 b)
{
	return uint3(mod(a,b));
}
uint4 typed_mod(uint4 a,uint4 b)
{
	return uint4(mod(a,b));
}
uint4 typed_mod(uint4 a, uint b)
{
    return uint4(mod(a.x, b), mod(a.y, b), mod(a.z, b), mod(a.w, b));
}

#define fmod(a,b) (a-b*trunc(a/b))


#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET6883660##n
#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET6883661##n

#endif
#endif