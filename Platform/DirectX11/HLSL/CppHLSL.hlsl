#ifdef __cplusplus
#define cbuffer struct
#define uniform

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define texture(tex,texc) tex.Sample(samplerState,texc)

#define R0
typedef D3DXMATRIX float4x4;
typedef D3DXVECTOR4 float4;
typedef D3DXVECTOR3 float3;
typedef D3DXVECTOR2 float2;

#define ALIGN __declspec( align( 16 ) )
#else
#define R0 : register(b0)
#define ALIGN
#endif