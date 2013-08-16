#ifndef DEPTH_SL
#define DEPTH_SL

//Enable/define the following to use a simpler, faster depth-to-linear-fade-z transform.
#	define NEW_DEPTH_TO_LINEAR_FADE_DIST_Z
//Unlike the old implementation, it doesn't need to know about whether the projection is a reversed
//or regular proj, it's fewer operations, and it's clearly commented and documented.


float depthBufferToLinearFadeZ(in float depth, in float3 depthToLinFadeDistParams)
{
	//Usual perspective transform -
	// Pos     Proj
	// | _ |   | _  0  _ 0 |     |    _    |
	// | _ |   | 0  _  _ 0 |     |    _    |
	// |zVS| . | 0  0  a b |  =  |zVS.a + b|
	// | 1 |   | 0  0 -1 0 |     |  -zVS   |
	//
	//depth = (zVS.a+b)/-zVS
	//
	//zVS = -b / ( depth + a )
	//
	//linFadeDist = -zVS / fadeDistMetres
	//            = b / (depth*fadeDistMetres + a*fadeDistMetres)
	//            = paramX / (depth*paramY + paramZ)
	
	return depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
}

float depthBufferToLinearFadeZ(float depth, float nearZ, float farZ)
{
#ifdef REVERSE_DEPTH
	return nearZ*farZ/(nearZ+(farZ-nearZ)*depth);
#else
	return -nearZ*farZ/((farZ-nearZ)*depth-farZ);
#endif
}

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToFadeDistance(float depth,float3 depthToLinFadeDistParams,float nearZ,float farZ,vec2 xy,vec2 tanHalf)
{
#ifdef VISION
	float dist=depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef NEW_DEPTH_TO_LINEAR_FADE_DIST_Z
	float linearFadeDistanceZ = depthBufferToLinearFadeZ(depth,depthToLinFadeDistParams);
#else
	float linearFadeDistanceZ = depthBufferToLinearFadeZ(depth,nearZ,farZ);
#endif

	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	
	return fadeDist;
#endif
}


float2 viewportCoordToTexRegionCoord( in float2 iViewportCoord, in float4 iViewportToTexRegionScaleBias )
{
	return iViewportCoord * iViewportToTexRegionScaleBias.xy + iViewportToTexRegionScaleBias.zw;
}


#endif