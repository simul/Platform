#ifndef DEPTH_SL
#define DEPTH_SL

// Enable the following to use a 3-parameter depth conversion, for possible slight speed improvement
#define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z

float depthToLinearDistance(float depth,vec3 depthToLinFadeDistParams)
{
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	return linearFadeDistanceZ;
}
vec4 depthToLinearDistance(vec4 depth,vec3 depthToLinFadeDistParams)
{
	vec4 linearFadeDistanceZ = depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz);
	return linearFadeDistanceZ;
}
// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef VISION
	float dist=depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef REVERSE_DEPTH
	if(depth<=0)
		return 1.0;
#else
	if(depth>=1.0)
		return 1.0;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
#endif
}
float depthToFadeDistance(float depth,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(depth<=0)
		return 1.0;
#else
	if(depth>=1.0)
		return 1.0;
#endif
#ifdef VISION
	float dist=depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef REVERSE_DEPTH
	float linearFadeDistanceZ = nearZ*farZ/(nearZ+(farZ-nearZ)*depth);
#else
	float linearFadeDistanceZ = nearZ*farZ/(farZ-(farZ-nearZ)*depth);
#endif
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	
	return fadeDist;
#endif
}

float fadeDistanceToDepth(float dist,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return 1.0;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z) / depthToLinFadeDistParams.y;
	return depth;
}


float fadeDistanceToDepth(float dist,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return 1.0;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
#ifdef REVERSE_DEPTH
	float depth				=((nearZ*farZ)/linearDistanceZ-nearZ)/(farZ-nearZ);
#else
	float depth				=((nearZ*farZ)/linearDistanceZ-farZ)/(nearZ-farZ);
#endif
	return depth;
}

vec2 viewportCoordToTexRegionCoord(vec2 iViewportCoord,vec4 iViewportToTexRegionScaleBias )
{
	return iViewportCoord * iViewportToTexRegionScaleBias.xy + iViewportToTexRegionScaleBias.zw;
}

#endif