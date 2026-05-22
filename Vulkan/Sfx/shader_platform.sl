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
vec4 mul(vec4 vec, mat4 mat){return vec * mat;}
vec3 mul(mat3 mat, vec3 vec){return mat * vec;}
vec3 mul(vec3 vec, mat3 mat){return vec * mat;}
vec2 mul(mat2 mat, vec2 vec){return mat * vec;}
vec2 mul(vec2 vec, mat2 mat){return vec * mat;}
mat3 mul(mat3 a, mat3 b){return a * b;}

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

// Ray Tracing

struct RayDesc
{
	vec3 Origin;
	float TMin;
	vec3 Direction;
	float TMax;
};

uint3 DispatchRaysIndex() { return gl_LaunchIDEXT; }
uint3 DispatchRaysDimensions() { return gl_LaunchSizeEXT; }
uint PrimitiveIndex() { return gl_PrimitiveID; }
uint InstanceIndex() { return gl_InstanceID; }
uint InstanceID() { return gl_InstanceCustomIndexEXT; }
uint GeometryIndex() { return gl_GeometryIndexEXT; }
float3 WorldRayOrigin() { return gl_WorldRayOriginEXT; }
float3 WorldRayDirection() { return gl_WorldRayDirectionEXT; }
float3 ObjectRayOrigin() { return gl_ObjectRayOriginEXT; }
float3 ObjectRayDirection() { return gl_ObjectRayDirectionEXT; }
float RayTMin() { return gl_RayTminEXT; }
uint RayFlags() { return gl_IncomingRayFlagsEXT; }
float RayTCurrent() { return gl_HitTEXT; }
uint HitKind() { return gl_HitKindEXT; }
float4x3 ObjectToWorld4x3() { return gl_ObjectToWorldEXT; }
float4x3 WorldToObject4x3() { return gl_WorldToObjectEXT; }
float3x4 WorldToObject3x4() { return gl_WorldToObject3x4EXT; }
float3x4 ObjectToWorld3x4() { return gl_ObjectToWorld3x4EXT;}

#define RAY_FLAG_NONE gl_RayFlagsNoneEXT
#define RAY_FLAG_FORCE_OPAQUE gl_RayFlagsOpaqueEXT
#define RAY_FLAG_FORCE_NON_OPAQUE gl_RayFlagsNoOpaqueEXT
#define RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH gl_RayFlagsTerminateOnFirstHitEXT
#define RAY_FLAG_SKIP_CLOSEST_HIT_SHADER gl_RayFlagsSkipClosestHitShaderEXT
#define RAY_FLAG_CULL_BACK_FACING_TRIANGLES gl_RayFlagsCullBackFacingTrianglesEXT
#define RAY_FLAG_CULL_FRONT_FACING_TRIANGLES gl_RayFlagsCullFrontFacingTrianglesEXT
#define RAY_FLAG_CULL_OPAQUE gl_RayFlagsCullOpaqueEXT
#define RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES gl_RayFlagsSkipAABBEXT
#define HIT_KIND_TRIANGLE_FRONT_FACE gl_HitKindFrontFacingTriangleEXT
#define HIT_KIND_TRIANGLE_BACK_FACE gl_HitKindBackFacingTriangleEXT

// Wave Intrinsics

uint WaveGetLaneCount()
{
	return gl_SubgroupSize;
}
uint WaveGetLaneIndex()
{
	return gl_SubgroupInvocationID.x;
}

#endif
#endif

