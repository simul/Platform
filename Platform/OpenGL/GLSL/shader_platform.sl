#ifndef SPGLSL_SL
#define SPGLSL_SL
#include "CppGlsl.hs"
// Note: array buffers in HLSL become interface blocks in GLSL which need std430, so vs_4_0 etc must require GLSL 4.3
profile vs_4_0(430);
profile gs_4_0(430);
profile ps_4_0(430);
profile cs_4_0(430);
profile vs_5_0(430);
profile ps_5_0(430);
profile gs_5_0(430);
profile cs_5_0(430);
#define SV_DispatchThreadID gl_GlobalInvocationID 
#define SV_VertexID gl_VertexID

#define float4 vec4
#define float3 vec3
#define float2 vec2

void sincos(float angle_rads,out float ty,out float tx)
{
	ty=sin(angle_rads);
	tx=cos(angle_rads);
}


#endif

