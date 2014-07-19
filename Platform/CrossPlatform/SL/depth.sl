#ifndef DEPTH_SL
#define DEPTH_SL
#define UNITY_DIST 1.0;
// Enable the following to use a 3-parameter depth conversion, for possible slight speed improvement
#define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z

#ifdef __PSSL__
#ifdef REVERSE_DEPTH
#define REVERSE_DEPTH1
#endif
#else
#if REVERSE_DEPTH==1
#define REVERSE_DEPTH1
#endif
#endif

float depthToLinearDistance(float depth,vec4 depthToLinFadeDistParams)
{
#ifdef REVERSE_DEPTH1
	if(depth<=0)
		return UNITY_DIST;//max_fade_distance_metres;
#else
	if(depth>=1.0)
		return UNITY_DIST;//max_fade_distance_metres;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x/(depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z)+depthToLinFadeDistParams.w*depth;
	return linearFadeDistanceZ;
}

vec4 depthToLinearDistance(vec4 depth,vec4 depthToLinFadeDistParams)
{
	vec4 linearFadeDistanceZ = depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz)+depthToLinFadeDistParams.wwww*depth;
#ifdef REVERSE_DEPTH1
	vec4 st=step(depth,vec4(0.0,0.0,0.0,0.0));
	linearFadeDistanceZ*=(vec4(1.0,1.0,1.0,1.0)-st);
	linearFadeDistanceZ+=st;
#else
	linearFadeDistanceZ=min(vec4(1.0,1.0,1.0,1.0),linearFadeDistanceZ);
#endif
	return linearFadeDistanceZ;
}

vec2 depthToLinearDistance(vec2 depth,vec4 depthToLinFadeDistParams)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz)+depthToLinFadeDistParams.ww*depth;
#ifdef REVERSE_DEPTH1
	vec2 st=step(depth,vec2(0.0,0.0));
	linearFadeDistanceZ*=(vec2(1.0,1.0)-st);
	linearFadeDistanceZ+=st;
#else
	linearFadeDistanceZ=min(vec2(1.0,1.0),linearFadeDistanceZ);
#endif
	return linearFadeDistanceZ;
}

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,vec2 xy,vec4 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH1
	if(depth<=0)
		return UNITY_DIST;
#else
	if(depth>=1.0)
		return UNITY_DIST;
#endif
	float linearFadeDistanceZ = depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z)+depthToLinFadeDistParams.w*depth;
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
}

vec2 depthToFadeDistance(vec2 depth,vec2 xy,vec4 depthToLinFadeDistParams,vec2 tanHalf)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz)+depthToLinFadeDistParams.ww*depth;
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	vec2 fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
#ifdef REVERSE_DEPTH1
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

float fadeDistanceToDepth(float dist,vec2 xy,vec4 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef REVERSE_DEPTH1
	if(dist>=1.0)
		return 0.0;
#else
	if(dist>=1.0)
		return UNITY_DIST;
#endif
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =0;
#if 0
	float invDepth=depthToLinFadeDistParams.y/((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z)
					+depthToLinFadeDistParams.w/(linearDistanceZ - depthToLinFadeDistParams.x);
	depth=1.0/invDepth;
#else
	if(depthToLinFadeDistParams.y>0)
		depth=((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z) / depthToLinFadeDistParams.y;
	else // LINEAR DEPTH case:
		depth=(linearDistanceZ - depthToLinFadeDistParams.x) / depthToLinFadeDistParams.w;
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