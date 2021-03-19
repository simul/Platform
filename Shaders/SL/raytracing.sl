//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short  Long    Description
//RG     rgen    for a ray generation shader
//RI     rint    for a ray intersection shader
//RA     rahit   for a ray any hit shader
//RH     rchit   for a ray closest hit shader
//RM     rmiss   for a ray miss shader
//RC     rcall   for a ray callable shader

#ifndef SIMUL_CROSSPLATFORM_RAYTRACING_SL
#define SIMUL_CROSSPLATFORM_RAYTRACING_SL

bool CheckSelfIntersection(float rayTMin, float rayTCurrent)
{
	return (rayTMin > rayTCurrent);
}

vec3 GetCurrentRayPostion(vec3 rayOrigin, vec3 rayDirection, float rayTCurrent)
{
	return (rayOrigin + (rayTCurrent * rayDirection));
}

vec3 GetCurrentIntersectionPosition(vec3 rayOrigin, vec3 rayDirection, float rayTCurrent, float rayTMin)
{
	//If less than TMin, return the origin
	if (CheckSelfIntersection(rayTMin, rayTCurrent))
		rayTCurrent = 0.0;

	return GetCurrentRayPostion(rayOrigin, rayDirection, rayTCurrent);
}

#endif