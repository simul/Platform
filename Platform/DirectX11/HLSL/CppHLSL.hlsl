#ifdef __cplusplus
#define cbuffer struct
#define uniform
#define R0
typedef D3DXMATRIX float4x4;
typedef D3DXVECTOR4 float4;
typedef D3DXVECTOR3 float3;
typedef D3DXVECTOR2 float2;
#else
#define R0 : register(b0)
#endif