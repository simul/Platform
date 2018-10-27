#ifndef CPP_GLSL
#define CPP_GLSL

#include "../../CrossPlatform/SL/CppSl.hs"

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

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#endif
#endif
