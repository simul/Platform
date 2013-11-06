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
vec2 depthToLinearDistance(vec2 depth,vec3 depthToLinFadeDistParams)
{
	vec2 linearFadeDistanceZ = depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz);
	return linearFadeDistanceZ;
}
// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
#ifdef VISION
	float dist=nearZ+depth*farZ;
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

#ifndef GLSL

void GetMSAACoordinates(Texture2DMS<float4> textureMS,vec2 texc,out int2 pos2,out int numSamples)
{
	uint2 dims;
	textureMS.GetDimensions(dims.x,dims.y,numSamples);
	pos2		=int2(texc*vec2(dims.xy));
}

// Blending (not just clouds but any low-resolution alpha-blended volumetric image) into a hi-res target.
// Requires a near and a far image, a low-res depth texture with far (true) depth in the x, near depth in the y and edge in the z;
// a hi-res MSAA true depth texture.
vec4 NearFarDepthCloudBlend(vec2 texCoords
							,Texture2D farImageTexture
							,Texture2D nearImageTexture
							,Texture2D lowResDepthTexture
							,Texture2DMS<float4> depthTextureMS
							,vec4 viewportToTexRegionScaleBias
							,vec3 depthToLinFadeDistParams)
{
	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 hires_depth_pos2;
	int numSamples;
	GetMSAACoordinates(depthTextureMS,depth_texc,hires_depth_pos2,numSamples);

	// First get the values that don't vary with MSAA sample:
	vec4 cloudFar;
	vec4 cloudNear		=vec4(0,0,0,1.0);
	vec4 lowres			=texture_clamp_lod(lowResDepthTexture,texCoords,0);
	float edge			=lowres.z;
	vec4 result;
	vec2 nearFarDist	=depthToLinearDistance(lowres.yx,depthToLinFadeDistParams);
	if(edge>0.0)
	{
		// At an edge we will do the interpolation for each MSAA sample.
		for(int i=0;i<numSamples;i++)
		{
			vec4 hiresDepth	=depthTextureMS.Load(hires_depth_pos2,i).x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			cloudNear		=depthDependentFilteredImage(nearImageTexture	,lowResDepthTexture,texCoords,vec4(0,1.0,0,0),depthToLinFadeDistParams,trueDist);
			cloudFar		=depthDependentFilteredImage(farImageTexture	,lowResDepthTexture,texCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist);
			float interp	=edge*saturate((nearFarDist.y-trueDist)/(nearFarDist.y-nearFarDist.x));
			result			+=lerp(cloudFar,cloudNear,interp);
		}
		result/=float(numSamples);
	}
	else
	{
		// Just use the zero MSAA sample if we're not at an edge:
		vec4 hiresDepth	=depthTextureMS.Load(hires_depth_pos2,0).x;
		float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		result			=depthDependentFilteredImage(farImageTexture,lowResDepthTexture,texCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist);
	}
	result.rgb			*=exposure;
	//result.g=edge;
    return result;
}

#endif
#endif