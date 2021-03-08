//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short	Long	Description
//RG	rgen	for a ray generation shader
//RI	rint	for a ray intersection shader
//RA	rahit	for a ray any hit shader
//RC	rchit	for a ray closest hit shader
//RM	rmiss	for a ray miss shader
//RC	rcall	for a ray callable shader

#ifndef SIMUL_CROSSPLATFORM_RAYTRACING_CONSTANTS_SL
#define SIMUL_CROSSPLATFORM_RAYTRACING_CONSTANTS_SL

struct Raytracing_AABB
{
	float minX;
	float minY;
	float minZ;
	float maxX;
	float maxY;
	float maxZ;
};
#endif