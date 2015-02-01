#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

//#define DEBUG_COMPOSITING

#define EDGE_FACTOR (0.0002)

#ifndef PI
#define PI (3.1415926536)
#endif

void ExtendDepths(inout vec2 farthest_nearest,float d,bool reverseDepth)
{
	if(reverseDepth)
	{
		farthest_nearest.y	=max(farthest_nearest.y,d);
		farthest_nearest.x	=min(farthest_nearest.x,d);
	}
	else
	{
		farthest_nearest.y	=min(farthest_nearest.y,d);
		farthest_nearest.x	=max(farthest_nearest.x,d);
	}

}
void ExtendDepths(inout vec2 farthest_nearest,vec2 d,bool reverseDepth)
{
	if(reverseDepth)
	{
		farthest_nearest.y	=max(farthest_nearest.y,max(d.x,d.y));
		farthest_nearest.x	=min(farthest_nearest.x,min(d.x,d.y));
	}
	else
	{
		farthest_nearest.y	=min(farthest_nearest.y,min(d.x,d.y));
		farthest_nearest.x	=max(farthest_nearest.x,max(d.x,d.y));
	}

}
void ExtendDepths(inout vec2 farthest_nearest,vec4 d,bool reverseDepth)
{
	vec2 mx=vec2(max(d.x,d.z),max(d.y,d.w));
	vec2 mn=vec2(min(d.x,d.z),min(d.y,d.w));
	if(reverseDepth)
	{
		farthest_nearest.y	=max(farthest_nearest.y,max(mx.x,mx.y));
		farthest_nearest.x	=min(farthest_nearest.x,min(mn.x,mn.y));
	}
	else
	{
		farthest_nearest.y	=min(farthest_nearest.y,min(mn.x,mn.y));
		farthest_nearest.x	=max(farthest_nearest.x,max(mx.x,mx.y));
	}

}

vec2 depthToLinearDistanceM(vec2 depth,DepthIntepretationStruct depthInterpretationStruct,float max_dist)
{
	vec2 linearFadeDistanceZ = saturate(depthInterpretationStruct.depthToLinFadeDistParams.xx / (depth*depthInterpretationStruct.depthToLinFadeDistParams.yy + depthInterpretationStruct.depthToLinFadeDistParams.zz) + depthInterpretationStruct.depthToLinFadeDistParams.ww*depth);
	if(depthInterpretationStruct.reverseDepth)
	{
		vec2 st = step(depth,vec2(0.0,0.0));
		linearFadeDistanceZ *= (vec2(1.0,1.0) - st);
		linearFadeDistanceZ += st;
	}
	else
	{
		linearFadeDistanceZ.x = min(max_dist,linearFadeDistanceZ.x);
		linearFadeDistanceZ.y = min(max_dist,linearFadeDistanceZ.y);
	}

	return linearFadeDistanceZ;
}

vec4 HalfscaleInitial_MSAA(TEXTURE2DMS_FLOAT4 sourceMSDepthTexture,int2 source_dims,int2 source_offset,int2 cornerOffset,int numberOfSamples,int2 pos,DepthIntepretationStruct depthInterpretationStruct)
{
	int2 pos0			=pos*2;
	int2 pos1			=pos0-cornerOffset;
#ifdef DEBUG_COMPOSITING
	if(pos.x<3)
		return vec4(0,0,saturate((pos1.y%3)/2.0),0);
#endif
	int2 max_pos=source_dims-int2(3,3);
	int2 min_pos=int2(1,1);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
							,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2+=source_offset;
	vec2 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec2(1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec2(0.0,1.0);
	}

	for(int i=0;i<2;i++)
	{
		for(int j=0;j<2;j++)
		{
			int2 hires_pos		=pos2+int2(i,j);
			//if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
			//	continue;
			//for(int k=0;k<numberOfSamples;k++)
			int k=0;
			{
				float d				=IMAGE_LOAD_MSAA(sourceMSDepthTexture,hires_pos,k).x;
				if(depthInterpretationStruct.reverseDepth)
				{
					farthest_nearest.y=max(farthest_nearest.y,d);
					farthest_nearest.x=min(farthest_nearest.x,d);
				}
				else
				{
					farthest_nearest.y=min(farthest_nearest.y,d);
					farthest_nearest.x=max(farthest_nearest.x,d);
				}

			}
		}
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		if(depthInterpretationStruct.reverseDepth)
		{
		}
		else
		{
			// Force edge at far clip.
			if(farthest_nearest.x >= 1.0)
				farthest_nearest.x = 1.0;
			if(farthest_nearest.y >= 1.0)
				farthest_nearest.y = 1.0;
		}

		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy,depthInterpretationStruct,1.0);
		edge	=abs(fn.x-fn.y);
		//edge	=abs(farthest_nearest.x-farthest_nearest.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}
	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}

vec4 HalfscaleInitial(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,DepthIntepretationStruct depthInterpretationStruct,bool find_edges)
{
	int2 pos0			=int2(pos*2);
	int2 pos1			=int2(pos0)-int2(cornerOffset);

	int2 max_pos		=int2(source_dims)-int2(3,3);
	int2 min_pos		=int2(1,1);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
								,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2				+=int2(source_offset);
	//if(pos2.x<123)
	//	return vec4(0,0,saturate(float(pos2.y)/float(source_dims.y)),0);
	vec2 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec2(1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec2(0.0,1.0);
	}

	float d1				=IMAGE_LOAD(sourceDepthTexture,pos2+int2(1,1)).x;
	float d2				=IMAGE_LOAD(sourceDepthTexture,pos2+int2(0,1)).x;
	float d3				=IMAGE_LOAD(sourceDepthTexture,pos2+int2(1,0)).x;
	float d4				=IMAGE_LOAD(sourceDepthTexture,pos2+int2(0,0)).x;
	vec4 d					=vec4(d1,d2,d3,d4);
	vec2 dmin2				=min(d.xy,d.zw);
	float dmin				=min(dmin2.x,dmin2.y);
	vec2 dmax2				=max(d.xy,d.zw);
	float dmax				=max(dmax2.x,dmax2.y);
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest.y=max(farthest_nearest.y,dmax);
		farthest_nearest.x=min(farthest_nearest.x,dmin);
	}
	else
	{
		farthest_nearest.y=min(farthest_nearest.y,dmin);
		farthest_nearest.x=max(farthest_nearest.x,dmax);
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		if(depthInterpretationStruct.reverseDepth)
		{
		}
		else
		{
			// Force edge at far clip.
			if(farthest_nearest.x >= 1.0)
				farthest_nearest.x = 1.0;
			if(farthest_nearest.y >= 1.0)
				farthest_nearest.y = 1.0;
		}

		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy,depthInterpretationStruct,1.0);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}

	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}
vec4 Halfscale(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,DepthIntepretationStruct depthInterpretationStruct)
{
	int2 pos0			=int2(pos*2);
	int2 pos1			=int2(pos0)-int2(cornerOffset);

	int2 max_pos		=int2(source_dims)-int2(5,5);
	int2 min_pos		=int2(2,3);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
								,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2				+=int2(source_offset);
	vec2 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec2(1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec2(0.0,1.0);
	}

	for(int i=0;i<4;i++)
	{
		int2 pos3			=pos2+int2(i*2-2,0);
		vec2 d1				=IMAGE_LOAD(sourceDepthTexture,pos3+int2(0,-3)).xy;
		vec2 d2				=IMAGE_LOAD(sourceDepthTexture,pos3+int2(0,0)).xy;
		vec2 d3				=IMAGE_LOAD(sourceDepthTexture,pos3+int2(0,1)).xy;
		vec2 d4				=IMAGE_LOAD(sourceDepthTexture,pos3+int2(0,4)).xy;
		vec4 f				=vec4(d1.x,d2.x,d3.x,d4.x);
		vec4 n				=vec4(d1.y,d2.y,d3.y,d4.y);
		vec2 dmin2,dmax2;
		if(depthInterpretationStruct.reverseDepth)
		{
			dmin2			=min(f.xy,f.zw);
			dmax2			=max(n.xy,n.zw);
		}
		else
		{
			dmin2			=min(n.xy,n.zw);
			dmax2			=max(f.xy,f.zw);
		}

		float dmin			=min(dmin2.x,dmin2.y);
		float dmax			=max(dmax2.x,dmax2.y);
		if(depthInterpretationStruct.reverseDepth)
		{
			farthest_nearest.y	=max(farthest_nearest.y,dmax);
			farthest_nearest.x	=min(farthest_nearest.x,dmin);
		}
		else
		{
			farthest_nearest.y	=min(farthest_nearest.y,dmin);
			farthest_nearest.x	=max(farthest_nearest.x,dmax);
		}

	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		if(depthInterpretationStruct.reverseDepth)
		{
		}
		else
		{
			// Force edge at far clip.
			farthest_nearest.x=min(farthest_nearest.x,1.0);
			farthest_nearest.y=min(farthest_nearest.y,1.0);
		}

		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy,depthInterpretationStruct,1.0);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}
	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}
vec4 DownscaleDepthFarNear(Texture2D sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 cornerOffset,int2 pos,vec2 scale,DepthIntepretationStruct depthInterpretationStruct)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos0			=int2(pos*scale);
	int2 pos1			=int2(pos0)-int2(cornerOffset);
#ifdef DEBUG_COMPOSITING
	if(pos.x<3)
		return vec4(0,0,saturate((pos1.y%3)/2.0),0);
#endif
	int2 max_pos		=int2(source_dims)-int2(scale.x+1,scale.y+1);
	int2 min_pos		=int2(1,1);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x))
								,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2				+=int2(source_offset);
	vec2 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec2(1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec2(0.0,1.0);
	}

	for(int i=0;i<scale.x+2;i++)
	{
		for(int j=0;j<scale.y+2;j++)
		{
			int2 hires_pos		=pos2+int2(i-1,j-1);
			float d				=IMAGE_LOAD(sourceDepthTexture,hires_pos).x;
			if(depthInterpretationStruct.reverseDepth)
			{
				farthest_nearest.y=max(farthest_nearest.y,d);
				farthest_nearest.x=min(farthest_nearest.x,d);
			}
			else
			{
				farthest_nearest.y=min(farthest_nearest.y,d);
				farthest_nearest.x=max(farthest_nearest.x,d);
			}

		}
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		if(depthInterpretationStruct.reverseDepth)
		{
		}
		else
		{
			// Force edge at far clip.
			farthest_nearest.x=min(farthest_nearest.x,1.0);
			farthest_nearest.y=min(farthest_nearest.y,1.0);
		}

		vec2 fn = depthToLinearDistanceM(farthest_nearest.xy,depthInterpretationStruct,1.0);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
		farthest_nearest.xy=saturate(farthest_nearest.xy);
	}
	vec4 res=vec4(farthest_nearest,edge,0.0);
	return res;
}

vec4 DownscaleFarNearEdge(Texture2D sourceDepthTexture,int2 source_dims,int2 cornerOffset,int2 pos,int2 scale,DepthIntepretationStruct depthInterpretationStruct)
{
	// pos is the texel position in the target.
	int2 pos2=pos*scale;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.
	vec2 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		 farthest_nearest		=vec2(1.0,0.0);
	}
	else
	{
		 farthest_nearest		=vec2(0.0,1.0);
	}

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

			if(depthInterpretationStruct.reverseDepth)
			{
				vec2 f2				=vec2(min(f.x,f.y),min(f.z,f.w));
				farthest_nearest.x	=min(f2.x,f2.y);
				vec2 n2				=vec2(max(n.x,n.y),max(n.z,n.w));
				farthest_nearest.y	=max(n2.x,n2.y);
			}
			else
			{
				vec2 f2				=max(f.xy,f.zw);
				farthest_nearest.x	=max(f2.x,f2.y);
				vec2 n2				=min(n.xy,n.zw);
				farthest_nearest.y	=min(n2.x,n2.y);
			}
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
			vec2 d				=IMAGE_LOAD(sourceDepthTexture,hires_pos).xy;
			if(depthInterpretationStruct.reverseDepth)
			{
				if(d.y>farthest_nearest.y)
					farthest_nearest.y	=d.y;
				if(d.x<farthest_nearest.x)
					farthest_nearest.x	=d.x;
			}
			else
			{
				if(d.y<farthest_nearest.y)
					farthest_nearest.y	=d.y;
				if(d.x>farthest_nearest.x)
					farthest_nearest.x	=d.x;
			}

		}
	}
#endif
	float edge=0.0;
	if(farthest_nearest.y!=farthest_nearest.x)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthInterpretationStruct);
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	res=vec4(farthest_nearest,edge,0);
	return res;
}
#endif