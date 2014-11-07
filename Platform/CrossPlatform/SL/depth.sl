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
	float linearFadeDistanceZ = saturate(depthToLinFadeDistParams.x/(depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z)+depthToLinFadeDistParams.w*depth);
	return linearFadeDistanceZ;
}

vec4 depthToLinearDistance(vec4 depth,vec4 depthToLinFadeDistParams)
{
	vec4 linearFadeDistanceZ = saturate(depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz)+depthToLinFadeDistParams.wwww*depth);
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
	vec2 linearFadeDistanceZ =saturate(depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz)+depthToLinFadeDistParams.ww*depth);
#ifdef REVERSE_DEPTH1
	vec2 st=step(depth,vec2(0.0,0.0));
	linearFadeDistanceZ*=(vec2(1.0,1.0)-st);
	linearFadeDistanceZ+=st;
#else
	linearFadeDistanceZ=min(vec2(1.0,1.0),linearFadeDistanceZ);
#endif
	return linearFadeDistanceZ;
}

#if defined(GL_FRAGMENT_SHADER) || !defined(GLSL)
void discardOnFar(float depth)
{
#ifdef REVERSE_DEPTH1
	if(depth<=0)
		discard;
#else
	if(depth>=1.0)
		discard;
#endif
}
#endif

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
//	-	
// if depthToLinFadeDistParams.z=0
// (0.07,300000.0, 0, 0):
//									0->0.07/(300000*0).
//									i.e. infinite.
//									1->0.07/(300000) -> v small value.
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
	vec2 linearFadeDistanceZ = saturate(depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz)+depthToLinFadeDistParams.ww*depth);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	vec2 fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
#ifdef REVERSE_DEPTH1
	fadeDist.x=max(fadeDist.x,step(0.0,-depth.x));
	fadeDist.y=max(fadeDist.y,step(0.0,-depth.y));
#else
	fadeDist.x=max(fadeDist.x,step(1.0,depth.x));
	fadeDist.y=max(fadeDist.y,step(1.0,depth.y));
#endif
	return fadeDist;
}

float fadeDistanceToDepth(float dist,vec2 xy,vec4 depthToLinFadeDistParams,vec2 tanHalf)
{
	float Tx				=xy.x*tanHalf.x;
	float Ty				=xy.y*tanHalf.y;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =0;

	if(depthToLinFadeDistParams.y>0)
		depth=((depthToLinFadeDistParams.x / linearDistanceZ) - depthToLinFadeDistParams.z) / depthToLinFadeDistParams.y;
	else // LINEAR DEPTH case:
		depth=(linearDistanceZ - depthToLinFadeDistParams.x) / depthToLinFadeDistParams.w;
	
#ifdef REVERSE_DEPTH1
	depth=min(depth,1.0-step(1.0,dist));
	//if(dist>=1.0)
	//	return 0.0;
#else
	depth=max(depth,step(1.0,dist));
	//if(dist>=1.0)
	//	return UNITY_DIST;
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