//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef DEPTH_SL
#define DEPTH_SL
#define UNITY_DIST 1.0
// Enable the following to use a 3-parameter depth conversion, for possible slight speed improvement
#define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z


struct DepthInterpretationStruct
{
	vec4 depthToLinFadeDistParams;
	bool reverseDepth;
};

float GetAltTexCoord(float alt_km,float minSunlightAltitudeKm,float fadeAltitudeRangeKm)
{
	float diff_km				= alt_km - minSunlightAltitudeKm;
	float sun_alt_texc			=0.5+0.5*saturate(diff_km /fadeAltitudeRangeKm);
	sun_alt_texc				-=0.5*saturate(-diff_km /(minSunlightAltitudeKm+1.0));
	return sun_alt_texc;
}

//Normalised 
float depthToLinearDistance(float depth,DepthInterpretationStruct dis)
{
	if(dis.reverseDepth)
	{
		if(depth<=0)
			return UNITY_DIST;//max_fade_distance_metres;
	}
	else
	{
		if(depth>=1.0)
			return UNITY_DIST;//max_fade_distance_metres;
	}
	float linearFadeDistanceZ = saturate(dis.depthToLinFadeDistParams.x/(depth*dis.depthToLinFadeDistParams.y + dis.depthToLinFadeDistParams.z)+dis.depthToLinFadeDistParams.w*depth);
	return linearFadeDistanceZ;
}

//Normalised 
vec4 depthToLinearDistance(vec4 depth,DepthInterpretationStruct dis)
{
	vec4 linearFadeDistanceZ = saturate(dis.depthToLinFadeDistParams.xxxx / (depth*dis.depthToLinFadeDistParams.yyyy + dis.depthToLinFadeDistParams.zzzz)+dis.depthToLinFadeDistParams.wwww*depth);

	if(dis.reverseDepth)
	{
		vec4 st=step(depth,vec4(0.0,0.0,0.0,0.0));
		linearFadeDistanceZ*=(vec4(1.0,1.0,1.0,1.0)-st);
		linearFadeDistanceZ+=st;
	}
	else
	{
		linearFadeDistanceZ=min(vec4(1.0,1.0,1.0,1.0),linearFadeDistanceZ);
	}
	return linearFadeDistanceZ;
}

//Normalised 
vec2 depthToLinearDistance(vec2 depth,DepthInterpretationStruct dis)
{
	vec2 linearFadeDistanceZ =saturate(dis.depthToLinFadeDistParams.xx / (depth*dis.depthToLinFadeDistParams.yy + dis.depthToLinFadeDistParams.zz)+dis.depthToLinFadeDistParams.ww*depth);
	
	if(dis.reverseDepth)
	{
		linearFadeDistanceZ.x = max(linearFadeDistanceZ.x, step(0.0, -depth.x));
		linearFadeDistanceZ.y = max(linearFadeDistanceZ.y, step(0.0, -depth.y));
	}
	else
	{
		linearFadeDistanceZ.x = max(linearFadeDistanceZ.x, step(1.0, depth.x));
		linearFadeDistanceZ.y = max(linearFadeDistanceZ.y, step(1.0, depth.y));
	}
	return linearFadeDistanceZ;
}

#if defined(GL_FRAGMENT_SHADER) || !defined(GLSL)
void discardOnFar(float depth,bool reverseDepth)
{
	if(reverseDepth)
	{
		if(depth<=0)
			discard;
	}
	else
	{
		if(depth>=1.0)
			discard;
	}
}
void discardUnlessFar(float depth,bool reverseDepth)
{
	if(reverseDepth)
	{
		if (depth > 0)
			discard;
	}
	else
	{
		if (depth < 1.0)
			discard;
	}
}
#endif

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
//	-	
// if dis.depthToLinFadeDistParams.z=0
// (0.07,300000.0, 0, 0):
//									0->0.07/(300000*0).
//									i.e. infinite.
//									1->0.07/(300000) -> v small value.
float depthToFadeDistance(float depth,vec2 xy,DepthInterpretationStruct dis,vec4 tanHalf)
{
	if(dis.reverseDepth)
	{
		if(depth<=0)
			return UNITY_DIST;
	}
	else
	{
		if(depth>=1.0)
			return UNITY_DIST;
	}
	float linearFadeDistanceZ = dis.depthToLinFadeDistParams.x / (depth*dis.depthToLinFadeDistParams.y + dis.depthToLinFadeDistParams.z)+dis.depthToLinFadeDistParams.w*depth;
	float Tx=xy.x*tanHalf.x+tanHalf.z;
	float Ty=xy.y*tanHalf.y+tanHalf.w;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
}

vec2 depthToFadeDistance(vec2 depth,vec2 xy,DepthInterpretationStruct dis,vec4 tanHalf)
{
	vec2 linearFadeDistanceZ	=saturate(dis.depthToLinFadeDistParams.xx / (depth*dis.depthToLinFadeDistParams.yy + dis.depthToLinFadeDistParams.zz)+dis.depthToLinFadeDistParams.ww*depth);
	float Tx = xy.x*tanHalf.x + tanHalf.z;
	float Ty = xy.y*tanHalf.y + tanHalf.w;
	vec2 fadeDist				=linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	if(dis.reverseDepth)
	{
		fadeDist.x					=max(fadeDist.x,step(0.0,-depth.x));
		fadeDist.y					=max(fadeDist.y,step(0.0,-depth.y));
	}
	else
	{
		fadeDist.x					=max(fadeDist.x,step(1.0,depth.x));
		fadeDist.y					=max(fadeDist.y,step(1.0,depth.y));
	}
	return fadeDist;
}

vec4 depthToFadeDistance(vec4 depth,vec2 xy,DepthInterpretationStruct dis,vec4 tanHalf)
{
	vec4 linearFadeDistanceZ	=saturate(dis.depthToLinFadeDistParams.xxxx / (depth*dis.depthToLinFadeDistParams.yyyy + dis.depthToLinFadeDistParams.zzzz)+dis.depthToLinFadeDistParams.wwww*depth);
	float Tx					=xy.x*tanHalf.x + tanHalf.z;
	float Ty					=xy.y*tanHalf.y + tanHalf.w;
	vec4 fadeDist				=linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	if(dis.reverseDepth)
	{
		fadeDist.x					=max(fadeDist.x,step(0.0,-depth.x));
		fadeDist.y					=max(fadeDist.y,step(0.0,-depth.y));
		fadeDist.z					=max(fadeDist.z,step(0.0,-depth.z));
		fadeDist.w					=max(fadeDist.w,step(0.0,-depth.w));
	}
	else
	{
		fadeDist.x					=max(fadeDist.x,step(1.0,depth.x));
		fadeDist.y					=max(fadeDist.y,step(1.0,depth.y));
		fadeDist.z					=max(fadeDist.z,step(1.0,depth.z));
		fadeDist.w					=max(fadeDist.w,step(1.0,depth.w));
	}
	return fadeDist;
}

float fadeDistanceToDepth(float dist,vec2 xy,DepthInterpretationStruct dis,vec4 tanHalf)
{
	float Tx = xy.x*tanHalf.x + tanHalf.z;
	float Ty = xy.y*tanHalf.y + tanHalf.w;
	float linearDistanceZ	=dist/sqrt(1.0+Tx*Tx+Ty*Ty);
	float depth =0;

	if(dis.depthToLinFadeDistParams.y>0)
		depth=((dis.depthToLinFadeDistParams.x / linearDistanceZ) - dis.depthToLinFadeDistParams.z) / dis.depthToLinFadeDistParams.y;
	else // LINEAR DEPTH case:
		depth=(linearDistanceZ - dis.depthToLinFadeDistParams.x) / dis.depthToLinFadeDistParams.w;
	
	if(dis.reverseDepth)
	{
		depth=min(depth,1.0-step(1.0,dist));
		//if(dist>=1.0)
		//	return 0.0;
	}
	else
	{
		depth=max(depth,step(1.0,dist));
		//if(dist>=1.0)
		//	return UNITY_DIST;
	}
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
	numSamples	=int(nums);
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