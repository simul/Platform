#ifndef DEPTH_SL
#define DEPTH_SL

// This converts a z-buffer depth into a distance in the units of nearZ and farZ,
//	-	where usually nearZ and farZ will be factors of the maximum fade distance.
float depthToDistance(float depth,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
#ifdef VISION
	float dist=depth*farZ;
	if(depth>=1.0)
		dist=1.0;
	return dist;
#else
#ifdef REVERSE_DEPTH
	float z=nearZ*farZ/(nearZ+(farZ-nearZ)*depth);
#else
	float z=-nearZ*farZ/((farZ-nearZ)*depth-farZ);
#endif
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	float dist=z*sqrt(1.0+Tx*Tx+Ty*Ty);
#ifdef REVERSE_DEPTH
	if(depth<=0.0)
		dist=1.0;
#else
	if(depth>=1.0)
		dist=1.0;
#endif
	return dist;
#endif
}


vec2 viewportCoordToTexRegionCoord( in vec2 iViewportCoord, in vec4 iViewportToTexRegionScaleBias )
{
	return iViewportCoord * iViewportToTexRegionScaleBias.xy + iViewportToTexRegionScaleBias.zw;
}


#endif