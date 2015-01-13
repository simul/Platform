#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

//#define DEBUG_COMPOSITING

#if REVERSE_DEPTH==1
#define EDGE_FACTOR (0.0002)
#else
#define EDGE_FACTOR (0.001)
#endif

#ifndef PI
#define PI (3.1415926536)
#endif
// Find nearest and furthest depths in MSAA texture.
// sourceDepthTexture, sourceMSDepthTexture, and targetTexture are ALL the SAME SIZE.
vec4 MakeDepthFarNear(Texture2D<float4> sourceDepthTexture,Texture2DMS<float4> sourceMSDepthTexture,uint numberOfSamples,int2 pos,vec4 depthToLinFadeDistParams)
{
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(uint k=0;k<numberOfSamples;k++)
	{
		float d;
		if(numberOfSamples==1)
			d				=sourceDepthTexture[pos].x;
		else
			d				=sourceMSDepthTexture.Load(pos,k).x;
#if REVERSE_DEPTH==1
		if(d>nearest_depth)
			nearest_depth	=d;
		if(d<farthest_depth)
			farthest_depth	=d;
#else
		if(d<nearest_depth)
			nearest_depth	=d;
		if(d>farthest_depth)
			farthest_depth	=d;
#endif
	}
	float edge		=0.0;
	if(farthest_depth!=nearest_depth)
	{
		vec2 fn		=depthToLinearDistance(vec2(farthest_depth,nearest_depth),depthToLinFadeDistParams);
		edge		=fn.x-fn.y;
		edge		=step(EDGE_FACTOR,edge);
	}
	return vec4(farthest_depth,nearest_depth,edge,0.0);
}
void ExtendDepths(inout vec2 farthest_nearest,float d)
{
	#if REVERSE_DEPTH==1
		farthest_nearest.y	=max(farthest_nearest.y,d);
		farthest_nearest.x	=min(farthest_nearest.x,d);
	#else
		farthest_nearest.y	=min(farthest_nearest.y,d);
		farthest_nearest.x	=max(farthest_nearest.x,d);
	#endif
}
void ExtendDepths(inout vec2 farthest_nearest,vec2 d)
{
	#if REVERSE_DEPTH==1
		farthest_nearest.y	=max(farthest_nearest.y,max(d.x,d.y));
		farthest_nearest.x	=min(farthest_nearest.x,min(d.x,d.y));
	#else
		farthest_nearest.y	=min(farthest_nearest.y,min(d.x,d.y));
		farthest_nearest.x	=max(farthest_nearest.x,max(d.x,d.y));
	#endif
}
void ExtendDepths(inout vec2 farthest_nearest,vec4 d)
{
	vec2 mx=vec2(max(d.x,d.z),max(d.y,d.w));
	vec2 mn=vec2(min(d.x,d.z),min(d.y,d.w));
	#if REVERSE_DEPTH==1
		farthest_nearest.y	=max(farthest_nearest.y,max(mx.x,mx.y));
		farthest_nearest.x	=min(farthest_nearest.x,min(mn.x,mn.y));
	#else
		farthest_nearest.y	=min(farthest_nearest.y,min(mn.x,mn.y));
		farthest_nearest.x	=max(farthest_nearest.x,max(mx.x,mx.y));
	#endif
}

vec2 depthToLinearDistanceM(vec2 depth, vec4 depthToLinFadeDistParams,float max_dist)
{
	vec2 linearFadeDistanceZ = saturate(depthToLinFadeDistParams.xx / (depth*depthToLinFadeDistParams.yy + depthToLinFadeDistParams.zz) + depthToLinFadeDistParams.ww*depth);
#ifdef REVERSE_DEPTH1
	vec2 st = step(depth, vec2(0.0, 0.0));
	linearFadeDistanceZ *= (vec2(1.0, 1.0) - st);
	linearFadeDistanceZ += st;
#else
	linearFadeDistanceZ.x = min(max_dist, linearFadeDistanceZ.x);
	linearFadeDistanceZ.y = min(max_dist, linearFadeDistanceZ.y);
#endif
	return linearFadeDistanceZ;
}

vec4 DownscaleDepthFarNear2(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,vec4 depthToLinFadeDistParams)
{
	// pos is the texel position in the target.
	int2 pos2=pos*2;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.

#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<4;i++)
	{
		int2 hires_pos		=pos2+int2(i-1,-1);
		vec4 u				=vec4(	sourceDepthTexture[int2(hires_pos.x,hires_pos.y+0)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+1)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+2)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+3)].x);
		ExtendDepths(farthest_nearest,u.xyzw);
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthToLinFadeDistParams);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	vec4 res=vec4(farthest_nearest,edge,0);
	return res;
}



vec4 DownscaleDepthFarNear4(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos2					=pos*4;
	pos2		-=cornerOffset;
	int2 max_pos=source_dims-int2(7,7);
	int2 min_pos=int2(1,1);
	pos2		=int2	(max(min_pos.x,min(pos2.x,max_pos.x))
						,max(min_pos.y,min(pos2.y,max_pos.y)));
	pos2		+=source_offset;
#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<6;i++)
	{
		for(int j=0;j<6;j++)
		{
			int2 hires_pos		=pos2+int2(i-1,j-1);
			float d				=sourceDepthTexture[hires_pos].x;
#if REVERSE_DEPTH==1
				farthest_nearest.y=max(farthest_nearest.y,d);
				farthest_nearest.x=min(farthest_nearest.x,d);
#else
				farthest_nearest.y=min(farthest_nearest.y,d);
				farthest_nearest.x=max(farthest_nearest.x,d);
#endif
		}
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthToLinFadeDistParams);
		edge	=abs(fn.x-fn.y);
		//edge	=abs(farthest_nearest.x-farthest_nearest.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	return		vec4(farthest_nearest,edge,0.0);
}

vec4 DownscaleDepthFarNear_MSAA4(Texture2DMS<float4> sourceMSDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,vec2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos2					=int2(pos*scale);
	pos2-=cornerOffset;
	pos2=max(int2(1,1),min(pos2,int2(source_dims)-int2(3,3)));
	pos2+=source_offset;
#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<2;i++)
	{
		for(int j=0;j<2;j++)
		{
			int2 hires_pos		=pos2+int2(i,j);
			//if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
			//	continue;
			vec4 u				=vec4(sourceMSDepthTexture.Load(hires_pos,0).x
									,sourceMSDepthTexture.Load(hires_pos,1).x
									,sourceMSDepthTexture.Load(hires_pos,2).x
									,sourceMSDepthTexture.Load(hires_pos,3).x);
#if REVERSE_DEPTH==1
			vec2 v				=max(u.xy,u.zw);
			farthest_nearest.y	=max(v.x,v.y);
			vec2 m				=min(u.xy,u.zw);
			farthest_nearest.x	=min(m.x,m.y);
#else
			vec2 v				=max(u.xy,u.zw);
			farthest_nearest.x	=max(v.x,v.y);
			vec2 m				=min(u.xy,u.zw);
			farthest_nearest.y	=min(m.x,m.y);
#endif
		}
	}
	float edge=0.0;
#if REVERSE_DEPTH==1
#else
	// Force edge at far clip.
	if (farthest_nearest.x >= 1.0)
		farthest_nearest.x = 2.0;
#endif
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy, depthToLinFadeDistParams,1.1);
		edge	=abs(fn.x-fn.y);
		//edge	=abs(farthest_nearest.x-farthest_nearest.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.x=saturate(farthest_nearest.x);
	}
	return		vec4(farthest_nearest,edge,0.0);
}

vec4 DownscaleDepthFarNear_MSAA(Texture2DMS<float4> sourceMSDepthTexture,int2 source_dims,int2 source_offset,int2 cornerOffset,int numberOfSamples,int2 pos,int2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos0			=pos*scale;
	int2 pos1			=pos0-cornerOffset;
#ifdef DEBUG_COMPOSITING
	if(pos.x<3)
		return vec4(0,0,saturate((pos1.y%3)/2.0),0);
#endif
	int2 max_pos=source_dims-int2(scale.x+1,scale.y+1);
	int2 min_pos=int2(1,1);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
							,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2+=source_offset;
#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<scale.x+2;i++)
	{
		for(int j=0;j<scale.y+2;j++)
		{
			int2 hires_pos		=pos2+int2(i-1,j-1);
			//if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
			//	continue;
			//for(int k=0;k<numberOfSamples;k++)
			int k=0;
			{
				float d				=sourceMSDepthTexture.Load(hires_pos,k).x;
#if REVERSE_DEPTH==1
				farthest_nearest.y=max(farthest_nearest.y,d);
				farthest_nearest.x=min(farthest_nearest.x,d);
#else
				farthest_nearest.y=min(farthest_nearest.y,d);
				farthest_nearest.x=max(farthest_nearest.x,d);
#endif
			}
		}
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
#if REVERSE_DEPTH==1
#else
		// Force edge at far clip.
		if (farthest_nearest.x >= 1.0)
			farthest_nearest.x = 1.0;
		if (farthest_nearest.y >= 1.0)
			farthest_nearest.y = 1.0;
#endif
		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy, depthToLinFadeDistParams,1.0);
		edge	=abs(fn.x-fn.y);
		//edge	=abs(farthest_nearest.x-farthest_nearest.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}
	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}

vec4 DownscaleDepthFarNear(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,vec2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos0			=pos*scale;
	int2 pos1			=pos0-cornerOffset;
#ifdef DEBUG_COMPOSITING
	if(pos.x<3)
		return vec4(0,0,saturate((pos1.y%3)/2.0),0);
#endif
	int2 max_pos=source_dims-int2(scale.x+1,scale.y+1);
	int2 min_pos=int2(1,1);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
							,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2+=source_offset;
#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<scale.x+2;i++)
	{
		for(int j=0;j<scale.y+2;j++)
		{
			int2 hires_pos		=pos2+int2(i-1,j-1);
			float d				=sourceDepthTexture[hires_pos].x;
#if REVERSE_DEPTH==1
			farthest_nearest.y=max(farthest_nearest.y,d);
			farthest_nearest.x=min(farthest_nearest.x,d);
#else
			farthest_nearest.y=min(farthest_nearest.y,d);
			farthest_nearest.x=max(farthest_nearest.x,d);
#endif
		}
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
#if REVERSE_DEPTH==1
#else
		// Force edge at far clip.
		farthest_nearest.x=min(farthest_nearest.x,1.0);
		farthest_nearest.y=min(farthest_nearest.y,1.0);
#endif
		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy, depthToLinFadeDistParams,1.0);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}
	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}

void SpreadEdge(Texture2D<vec4> sourceDepthTexture,RWTexture2D<vec4> target2DTexture,uint2 pos)
{
	float e=0.0;
	for(int i=-1;i<2;i++)
	{
		for(int j=-1;j<2;j++)
		{
			e=max(e,sourceDepthTexture[pos.xy+uint2(i,j)].z);
		}
	}
	vec4 res=sourceDepthTexture[pos.xy];
	res.z=e;
	target2DTexture[pos.xy]=res;
}

vec4 DownscaleFarNearEdge(Texture2D<float4> sourceDepthTexture,int2 source_dims,int2 cornerOffset,int2 pos,int2 scale,vec4 depthToLinFadeDistParams)
{
	// pos is the texel position in the target.
	int2 pos2=pos*scale;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.

#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	vec4 res=vec4(0,0,0,0);
	pos2					-=cornerOffset;
	int2 minpos				=cornerOffset;
	int2 maxpos				=source_dims.xy-int2(2,2);
#if 0
	for(uint i=0;i<scale.x;i+=2)
	{
		for(uint j=0;j<scale.y;j+=2)
		{
			int2 hires_pos		=int2(pos2.x+i,pos2.y+j);
			// MUST do this in case we go slightly over the edge:
			//hires_pos.xy		=min(hires_pos.xy,maxpos);
			vec2 c11			=sourceDepthTexture[hires_pos			].xy;
			vec2 c12			=sourceDepthTexture[hires_pos+int2(0,1)	].xy;
			vec2 c21			=sourceDepthTexture[hires_pos+int2(1,0)	].xy;
			vec2 c22			=sourceDepthTexture[hires_pos+int2(1,1)	].xy;
			vec4 f				=vec4(	 c11.x
										,c12.x
										,c21.x
										,c22.x);
			vec4 n				=vec4(	 c11.y
										,c12.y
										,c21.y
										,c22.y);
			
#if REVERSE_DEPTH==1
			vec2 f2				=vec2(min(f.x,f.y),min(f.z,f.w));
			farthest_nearest.x	=min(f2.x,f2.y);
			vec2 n2				=vec2(max(n.x,n.y),max(n.z,n.w));
			farthest_nearest.y	=max(n2.x,n2.y);
#else
			vec2 f2				=max(f.xy,f.zw);
			farthest_nearest.x	=max(f2.x,f2.y);
			vec2 n2				=min(n.xy,n.zw);
			farthest_nearest.y	=min(n2.x,n2.y);
#endif
		}
	}
#else
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			int2 hires_pos		=int2(pos2.x+i,pos2.y+j); 
			// MUST do this in case we go slightly over the edge:
			hires_pos.x			=max(minpos.x,min(hires_pos.x,maxpos.x));
			hires_pos.y			=max(minpos.y,min(hires_pos.y,maxpos.y));
			vec2 d				=sourceDepthTexture[hires_pos].xy;
#if REVERSE_DEPTH==1
				if(d.y>farthest_nearest.y)
					farthest_nearest.y	=d.y;
				if(d.x<farthest_nearest.x)
					farthest_nearest.x	=d.x;
#else
				if(d.y<farthest_nearest.y)
					farthest_nearest.y	=d.y;
				if(d.x>farthest_nearest.x)
					farthest_nearest.x	=d.x;
#endif
		}
	}
#endif
	float edge=0.0;
	if(farthest_nearest.y!=farthest_nearest.x)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthToLinFadeDistParams);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	res=vec4(farthest_nearest,edge,0);
	return res;
}
#endif