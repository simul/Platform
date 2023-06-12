//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short  Long    Description
//RG     rgen    for a ray generation shader
//RI     rint    for a ray intersection shader
//RA     rahit   for a ray any hit shader
//RH     rchit   for a ray closest hit shader
//RM     rmiss   for a ray miss shader
//RC     rcall   for a ray callable shader

//Ray flags are passed to TraceRay() or RayQuery::TraceRayInline() to override transparency, culling, and early-out behavior.
//https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags
//RAY_FLAG_NONE								= 0x000,
//RAY_FLAG_FORCE_OPAQUE						= 0x001,
//RAY_FLAG_FORCE_NON_OPAQUE					= 0x002,
//RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH	= 0x004,
//RAY_FLAG_SKIP_CLOSEST_HIT_SHADER			= 0x008,
//RAY_FLAG_CULL_BACK_FACING_TRIANGLES		= 0x010,
//RAY_FLAG_CULL_FRONT_FACING_TRIANGLES		= 0x020,
//RAY_FLAG_CULL_OPAQUE						= 0x040,
//RAY_FLAG_CULL_NON_OPAQUE					= 0x080,
//RAY_FLAG_SKIP_TRIANGLES					= 0x100,
//RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES		= 0x200

//HitGroupRecordAddress Calculation:
//
//HitGroupRecordAddress
//= D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StartAddress						[from: DispatchRays()] 
//+ D3D12_DISPATCH_RAYS_DESC.HitGroupTable.StrideInBytes					[from: DispatchRays()] 
//* ( RayContributionToHitGroupIndex										[from: TraceRay()] 
//+ ( MultiplierForGeometryContributionToHitGroupIndex						[from: TraceRay()] 
//* GeometryContributionToHitGroupIndex )									[system generated index of geometry in bottom-level acceleration structure (0,1,2,3..)]
//+ D3D12_RAYTRACING_INSTANCE_DESC.InstanceContributionToHitGroupIndex )	[from: instance]
//
//HitGroupRecordAddress = HGT.StartAddress + HGT.Stride * (RayHGIdx + (GeoHGStride * GeoHGIdx) + InstanceHGIdx);

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

//Positive X = 0
//Negative X = 1
//Positive Y = 2
//Negative Y = 3
//Positive Z = 4
//Negative Z = 5
#define Raytracing_AABB_FaceIndex uint

//Constant Buffer Structs
NamedConstantBuffer<TraceRayParameters> traceRayParameters : register(b0)
{
	uint instanceInclusionMask;								//Inclusion Mask for instances
	uint rayContributionToHitGroupIndex;					//HitGroup Index for Ray into the SBT.
	uint multiplierForGeometryContributionToHitGroupIndex;	//HitGroup Stride for Geometry into the SBT.
	uint missShaderIndex;									//MissShader Index into the SBT.
};

#endif