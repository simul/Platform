//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short  Long    Description
//RG     rgen    for a ray generation shader
//RI     rint    for a ray intersection shader
//RA     rahit   for a ray any hit shader
//RH     rchit   for a ray closest hit shader
//RM     rmiss   for a ray miss shader
//RC     rcall   for a ray callable shader

#ifndef SIMUL_CROSSPLATFORM_RAYTRACING_CONSTANTS_SL
#define SIMUL_CROSSPLATFORM_RAYTRACING_CONSTANTS_SL

//Structures
struct Raytracing_AABB
{
	float minX;
	float minY;
	float minZ;
	float maxX;
	float maxY;
	float maxZ;
};

//Constant Bufffers

/*
HitGroupRecordAddress Calculation :

HitGroupRecordAddress
= D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StartAddress					[from: DispatchRays()] 
+ D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StrideInBytes					[from: DispatchRays()] 
* ( RayContributionToHitGroupIndex										[from: TraceRay()] 
+ ( MultiplierForGeometryContributionToHitGroupIndex					[from: TraceRay()] 
* GeometryContributionToHitGroupIndex )									[system generated index of geometry in bottom-level acceleration structure (0,1,2,3..)]
+ D3D12_RAYTRACING_INSTANCE_DESC.InstanceContributionToHitGroupIndex )	[from: instance]

HitGroupRecordAddress = HGT.StartAddress + HGT.Stride * (RayHGIdx + (GeoHGStride * GeoHGIdx) + InstanceHGIdx);
*/

SIMUL_CONSTANT_BUFFER(TraceRayParameters, 2)
	
uniform uint instanceInclusionMask;								//Inclusion Mask for instances
uniform uint rayContributionToHitGroupIndex;					//HitGroup Index for Ray into the SBT.
uniform uint multiplierForGeometryContributionToHitGroupIndex;	//HitGroup Stride for Geometry into the SBT.
uniform uint missShaderIndex;									//MissShader Index into the SBT.

SIMUL_CONSTANT_BUFFER_END

#endif