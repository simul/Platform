//Copyright (c) 2021 Simul Software Ltd. All rights reserved.

//Short  Long    Description
//RG     rgen    for a ray generation shader
//RI     rint    for a ray intersection shader
//RA     rahit   for a ray any hit shader
//RH     rchit   for a ray closest hit shader
//RM     rmiss   for a ray miss shader
//RC     rcall   for a ray callable shader

#include "shader_platform.sl"
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

bool InsideRaytracing_AABB(Raytracing_AABB aabb, vec3 position)
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

int3 GetRaytracing_AABBGridSizeSigned(Raytracing_AABB aabb, vec3 cloudWindowSizeKm, vec3 mapSize)
{
	vec3 minKm = vec3(aabb.minX, aabb.minY, aabb.minZ) / vec3(1000.0, 1000.0, 1000.0);
	vec3 maxKm = vec3(aabb.maxX, aabb.maxY, aabb.maxZ) / vec3(1000.0, 1000.0, 1000.0);

	vec3 sizeKm = maxKm - minKm;
	return int3(sizeKm * (mapSize / cloudWindowSizeKm));
}

vec3 GetRaytracing_AABBGridTexCoord(Raytracing_AABB aabb, vec3 cloudWindowSizeKm, vec3 mapSize, vec3 positionKm)
{
	vec3 minKm = vec3(aabb.minX, aabb.minY, aabb.minZ) / vec3(1000.0, 1000.0, 1000.0);

	vec3 positionOffsetKm = positionKm - minKm;
	return positionOffsetKm * (mapSize / cloudWindowSizeKm);
}

int3 GetRaytracing_AABBGridPixel(Raytracing_AABB aabb, vec3 cloudWindowSizeKm, vec3 mapSize, vec3 positionKm)
{
	return int3(GetRaytracing_AABBGridTexCoord(aabb, cloudWindowSizeKm, mapSize, positionKm));
}

vec3 GetPositionFromRaytracing_AABBGridPixel(Raytracing_AABB aabb, vec3 cloudWindowSizeKm, vec3 mapSize, int3 gridPixel)
{
	vec3 gridPixelF = gridPixel; 
	//Centralise.
	gridPixelF.x += (gridPixel.x < 0 ? -0.5 : +0.5);
	gridPixelF.y += (gridPixel.y < 0 ? -0.5 : +0.5);
	gridPixelF.z += (gridPixel.z < 0 ? -0.5 : +0.5);

	vec3 positionOffsetKm = gridPixelF / (mapSize / cloudWindowSizeKm);
	vec3 minKm = vec3(aabb.minX, aabb.minY, aabb.minZ) / vec3(1000.0, 1000.0, 1000.0);

	return minKm + positionOffsetKm;
}

int3 GetGridStepDirection(vec3 direction)
{
	return int3(
		(direction.x < 0.0 ? -1 : 1),
		(direction.y < 0.0 ? -1 : 1),
		(direction.z < 0.0 ? -1 : 1)
	);
}

int3 GetGridNormal(vec3 direction)
{
	vec3 D = abs(direction); //Reduce to one octree
	float e = min(min(D.x, D.y), D.z);
	vec3 N = step(D, vec3(e, e, e));
	return int3(normalize(N * direction));
}

vec3 InitAccumlatedScaleXYZ(int3 gridStep, vec3 gridStartTC, int3 gridStart, vec3 scaleXYZ)
{
	vec3 accumlatedScaleXYZ;

	
	if (gridStep.x == -1)
		accumlatedScaleXYZ.x = (gridStartTC.x - float(gridStart.x)) * scaleXYZ.x;
	else
		accumlatedScaleXYZ.x = (float(gridStart.x + 1) - gridStartTC.x) * scaleXYZ.x;

	if (gridStep.y == -1)
		accumlatedScaleXYZ.y = (gridStartTC.y - float(gridStart.y)) * scaleXYZ.y;
	else
		accumlatedScaleXYZ.y = (float(gridStart.y + 1) - gridStartTC.y) * scaleXYZ.y;

	if (gridStep.z == -1)
		accumlatedScaleXYZ.z = (gridStartTC.z - float(gridStart.z)) * scaleXYZ.z;
	else
		accumlatedScaleXYZ.z = (float(gridStart.z + 1) - gridStartTC.z) * scaleXYZ.z;
		
	return accumlatedScaleXYZ;
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
		result.T = dot(position - rayOrigin, normal) / denom;
	else
		result.T = 0.0;
	
	return result;
}

struct AABBIntersectionResult
{
	vec3 position;
	float rayLength;
	Raytracing_AABB_FaceIndex face;
};
struct AABBIntersectionResults
{
	AABBIntersectionResult primary;
	AABBIntersectionResult secondary;
};

AABBIntersectionResults GetAABBIntersection(Raytracing_AABB aabb, vec3 rayOrigin, vec3 rayDirection)
{
	AABBIntersectionResults result;
	
	bool primary = false;
	bool secondary = false;
	for (uint i = 0; i < 6; i++)
	{
		vec3 N = GetRaytracing_AABB_FaceNormal(Raytracing_AABB_FaceIndex(i));
		vec3 P = GetRaytracing_AABB_FaceMidpoint(aabb, Raytracing_AABB_FaceIndex(i));
		RayPlaneIntersectionResult rpiResult = GetRayPlaneIntersection(rayOrigin, rayDirection, N, P);

		if (rpiResult.intersection)
		{
			bool primaryIntersection = dot(N, rayDirection) > 0 ? false : true;
			if (primaryIntersection)
			{
			
				result.primary.position = GetCurrentRayPostion(rayOrigin, rayDirection, rpiResult.T);
				result.primary.rayLength = rpiResult.T;
				result.primary.face = Raytracing_AABB_FaceIndex(i);
				primary = true;
			}
			else
			{
				result.secondary.position = GetCurrentRayPostion(rayOrigin, rayDirection, rpiResult.T);
				result.secondary.rayLength = rpiResult.T;
				result.secondary.face = Raytracing_AABB_FaceIndex(i);
				secondary = true;
			}	
		}

		if (primary && secondary)
		{
			return result;
		}

		}
	return result;
}

#endif