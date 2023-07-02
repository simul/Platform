#ifndef CPP_GLSL
#define CPP_GLSL

#include "CppSl.sl"

#ifndef __cplusplus

#define USE_D3D_REF_MODE 0

// This should be passed to the compiler, do we use it? (gaussian.sfx)
#define SCAN_SMEM_SIZE 128
#define THREADS_PER_GROUP 128

float saturate(float value)	{return clamp(value, 0.0, 1.0);}
vec2 saturate(vec2 vvalue)	{return clamp(vvalue,vec2(0.0,0.0),vec2(1.0,1.0));}
vec3 saturate(vec3 vvalue)	{return clamp(vvalue,vec3(0.0,0.0,0.0),vec3(1.0,1.0,1.0));}
vec4 saturate(vec4 vvalue)	{return clamp(vvalue,vec4(0.0,0.0,0.0,0.0),vec4(1.0,1.0,1.0,1.0));}

vec4 mul(mat4 mat, vec4 vec){return mat * vec;}
vec4 mul(vec4 vec, mat4 mat){return vec * mat;}
vec3 mul(mat3 mat, vec3 vec){return mat * vec;}
vec3 mul(vec3 vec, mat3 mat){return vec * mat;}
vec2 mul(mat2 mat, vec2 vec){return mat * vec;}
vec2 mul(vec2 vec, mat2 mat){return vec * mat;}

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4


uint reversebits(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return bits;
}

float reverse_y_coord(float a)
{
	return a;
}

vec2 reverse_y_coord(vec2 a)
{
	return vec2(a.x,1.0-a.y);
}

vec3 reverse_y_coord(vec3 a)
{
	return a;//vec3(a.x,1.0-a.y,a.z);
}

vec4 reverse_y_coord(vec4 a)
{
	return a;//vec4(a.x,1.0-a.y,a.z,a.w);
}

#define fmod(a,b) (a-b*trunc(a/b))

#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_0(n) : SV_TARGET6883660##n
#define SIMUL_RENDERTARGET_OUTPUT_DSB_INDEX_1(n) : SV_TARGET6883661##n

#endif
#endif