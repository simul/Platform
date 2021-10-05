//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short  Long    Description
//RG     rgen    for a ray generation shader
//RI     rint    for a ray intersection shader
//RA     rahit   for a ray any hit shader
//RH     rchit   for a ray closest hit shader
//RM     rmiss   for a ray miss shader
//RC     rcall   for a ray callable shader

#include "raytracing_constants.sl"

#ifndef SIMUL_CROSSPLATFORM_RAYTRACING_SL
#define SIMUL_CROSSPLATFORM_RAYTRACING_SL


bool CheckSelfIntersection(float rayTMin, float rayTCurrent)
{
	return (rayTMin > rayTCurrent);
}

vec3 GetCurrentRayPostion(vec3 rayOrigin, vec3 rayDirection, float rayTCurrent)
{
	return (rayOrigin + (rayTCurrent * normalize(rayDirection)));
}

vec3 GetCurrentIntersectionPosition(vec3 rayOrigin, vec3 rayDirection, float rayTCurrent, float rayTMin)
{
	//If less than TMin, return the origin
	if (CheckSelfIntersection(rayTMin, rayTCurrent))
		rayTCurrent = 0.0;

	return GetCurrentRayPostion(rayOrigin, rayDirection, rayTCurrent);
}

bool InsideAABB(Raytracing_AABB aabb, vec3 position)
{
	if (position.x > aabb.minX &&
		position.y > aabb.minY &&
		position.z > aabb.minZ &&
		position.x < aabb.maxX &&
		position.y < aabb.maxY &&
		position.z < aabb.maxZ)
		return true;
	else
		return false;
}

vec3 GetRaytracing_AABB_FaceNormal(Raytracing_AABB_FaceIndex index)
{
	switch (index)
	{
		case 0:
			return vec3(+1, 0, 0);
		case 1:
			return vec3(-1, 0, 0);
		case 2:
			return vec3(0, +1, 0);
		case 3:
			return vec3(0, -1, 0);
		case 4:
			return vec3(0, 0, +1);
		case 5:
			return vec3(0, 0, -1);
		default:
			return vec3(0, 0, 0);
	}
}
vec3 GetRaytracing_AABB_FaceMidpoint(Raytracing_AABB aabb, Raytracing_AABB_FaceIndex index)
{
	uint id = uint(index);
	bool positiveFace = (id % 2 == 0);
	bool xFace = (id % 3 == 0);
	bool yFace = (id % 3 == 1);
	bool zFace = (id % 3 == 2);
	vec3 min = vec3(aabb.minX, aabb.minY, aabb.minZ);
	vec3 max = vec3(aabb.maxX, aabb.maxY, aabb.maxZ);
	
	if (xFace)
	{
		if(positiveFace)
			min.x = max.x;
		else
			max.x = min.x;
	}
	else if (yFace)
	{
		if (positiveFace)
			min.y = max.y;
		else
			max.y = min.y;
	}
	else if (zFace)
	{
		if (positiveFace)
			min.z = max.z;
		else
			max.z = min.z;
	}
	else
		return vec3(0, 0, 0);
	
	return 0.5 * (min + max);
}

struct RayPlaneIntersectionResult
{
	float T;
	bool intersection;
};
RayPlaneIntersectionResult GetRayPlaneIntersection(vec3 rayOrigin, vec3 rayDirection, vec3 normal, vec3 position)
{
	RayPlaneIntersectionResult result;
	
	float denom = dot(rayDirection, normal);
	result.intersection = (denom != 0);
	
	if (result.intersection)
		result.T = dot(position - rayDirection, normal) / denom;
	else
		result.T = 0.0;
	
	return result;
}

struct AABBIntersectionResult
{
	vec3 position;
	Raytracing_AABB_FaceIndex face;
	float rayLength;
};
AABBIntersectionResult GetAABBIntersection(Raytracing_AABB aabb, vec3 rayOrigin, vec3 rayDirection)
{
	AABBIntersectionResult result;
	
	bool possiblePrimaryIntersectionPlanes[6];
	for (uint i = 0; i < 6; i++)
	{
		vec3 N = GetRaytracing_AABB_FaceNormal(Raytracing_AABB_FaceIndex(i));
		float dotResult = dot(N, rayDirection);
		possiblePrimaryIntersectionPlanes[i] = dotResult > 0 ? false : true;
	}
	for (uint j = 0; j < 6; j++)
	{
		if (possiblePrimaryIntersectionPlanes[j])
		{
			RayPlaneIntersectionResult rpiResult;
			vec3 N = GetRaytracing_AABB_FaceNormal(Raytracing_AABB_FaceIndex(j));
			vec3 P = GetRaytracing_AABB_FaceMidpoint(aabb, Raytracing_AABB_FaceIndex(j));
			rpiResult = GetRayPlaneIntersection(rayOrigin, rayDirection, N, P);

			if (rpiResult.intersection)
			{
				result.position = GetCurrentRayPostion(rayOrigin, rayDirection, rpiResult.T);
				result.rayLength = rpiResult.T;
				result.face = Raytracing_AABB_FaceIndex(j);
				return result;
			}
		}
	}
}

#endif