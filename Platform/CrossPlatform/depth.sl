#ifndef DEPTH_SL
#define DEPTH_SL

float depthToDistance(float depth,vec2 xy,float nearZ,float farZ,vec2 tanHalf)
{
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
}

#endif