#include "CppHlsl.hlsl"
cbuffer GpuSkyConstants R0
{
	float3 lightDir;
	float4 mieRayleighRatio;
	float2 texelOffsets;
	float hazeEccentricity;
};