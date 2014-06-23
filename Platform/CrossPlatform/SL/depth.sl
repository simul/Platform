#ifndef DEPTH_SL
#define DEPTH_SL
#define UNITY_DIST 1.0;
// Enable the following to use a 3-parameter depth conversion, for possible slight speed improvement
#define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z

float depthToLinearDistance(float depth,vec3 depthToLinFadeDistParams)
{
#if REVERSE_DEPTH==1
	if(depth<=0)
		return UNITY_DIST;//max_fade_distance_metres;
#else
	if(depth>=1.0)
		return UNITY_DIST;//max_fade_distance_metres;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	return linearFadeDistanceZ;
}

vec4 depthToLinearDistance(vec4 depth,vec3 depthToLinFadeDistParams)
{
	vec4 linearFadeDistanceZ = depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz);
	
#if REVERSE_DEPTH==1
	vec4 st=step(depth,0.0);
	linearFadeDistanceZ*=(vec4(1.0,1.0,1.0,1.0)-st);
	linearFadeDistanceZ+=st;
#else
	linearFadeDistanceZ=min(vec4(1.0,1.0,1.0,1.0),linearFadeDistanceZ);
#endif
	return linearFadeDistanceZ;
}

vec2 depthToLinearDistance(vec2 depth,vec3 depthToLinFadeDistParams)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz);
#if REVERSE_DEPTH==1
	vec2 st=step(depth,0.0);
	linearFadeDistanceZ*=(vec2(1.0,1.0)-st);
	linearFadeDistanceZ+=st;
#else
	linearFadeDistanceZ=min(vec2(1.0,1.0),linearFadeDistanceZ);
#endif
	return linearFadeDistanceZ;
}

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#if REVERSE_DEPTH==1
	if(depth<=0)
		return UNITY_DIST;
#else
	if(depth>=1.0)
		return UNITY_DIST;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
}

vec2 depthToFadeDistance(vec2 depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	vec2 fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
#if REVERSE_DEPTH==1
	if(depth.x<=0)
		fadeDist.x=1.0;
	if(depth.y<=0)
		fadeDist.y=1.0;
#else
	if(depth.x>=1.0)
		fadeDist.x=1.0;
	if(depth.y>=1.0)
		fadeDist.y=1.0;
#endif
	return fadeDist;
}

float fadeDistanceToDepth(float dist,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#if REVERSE_DEPTH==1
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return UNITY_DIST;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z) / depthToLinFadeDistParams.y;
	return depth;
}

float fadeDistanceToDepth(float dist,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#if REVERSE_DEPTH==1
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return UNITY_DIST;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
#if REVERSE_DEPTH==1
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

#ifndef GLSL

void GetCoordinates(Texture2D t,vec2 texc,out int2 pos2)
{
	uint2 dims;
	t.GetDimensions(dims.x,dims.y);
	pos2		=int2(texc*vec2(dims.xy));
}

void GetMSAACoordinates(Texture2DMS<vec4> textureMS,vec2 texc,out int2 pos2,out int numSamples)
{
	uint2 dims;
	uint nums;
	textureMS.GetDimensions(dims.x,dims.y,nums);
	numSamples=nums;
	pos2		=int2(texc*vec2(dims.xy));
}

void GetMSAACoordinates(Texture2DMS<vec4> textureMS,vec2 texc,out int2 pos2,out uint numSamples)
{
	uint2 dims;
	textureMS.GetDimensions(dims.x,dims.y,numSamples);
	pos2		=int2(texc*vec2(dims.xy));
}
#endif
#endif