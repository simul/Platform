//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

//#define DEBUG_COMPOSITING

#define EDGE_FACTOR (0.002)

// A smooth transition from near to far over full height of a 1920x1080 screen would be 1/1080 per pixel = 0.001

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

vec4 HalfscaleInitial_MSAA(TEXTURE2DMS_FLOAT4 sourceMSDepthTexture,int2 source_dims,int2 max_dims
	,int2 source_offset,int2 cornerOffset,int2 pos
	,DepthIntepretationStruct depthInterpretationStruct
	,float nearThresholdDepth)
{
	int2 pos0			=pos*2;
	int2 pos1			=pos0-cornerOffset+max_dims;
	pos1				=pos1%max_dims;
#ifdef DEBUG_COMPOSITING
	if(pos.x<3)
		return vec4(0,0,saturate((pos1.y%3)/2.0),0);
#endif
	int2 max_pos	=source_dims-int2(3,3);
	int2 min_pos	=int2(1,1);
	int2 pos2		=int2(max(min_pos.x,min(pos1.x,max_pos.x))
							,max(min_pos.y,min(pos1.y,max_pos.y)));
	pos2			+=source_offset;
	vec4 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec4(1.0,0.0,1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec4(0.0,1.0,0.0,1.0);
	}

	for(int i=0;i<2;i++)
	{
		for(int j=0;j<2;j++)
		{
			int2 hires_pos		=pos2+int2(i,j);
			int k=0;
			{
				vec2 d			=TEXTURE_LOAD_MSAA(sourceMSDepthTexture,hires_pos,k).xx;
				if(depthInterpretationStruct.reverseDepth)
					d.x					= lerp(d.x,0.0,saturate(0.01*(d.x-nearThresholdDepth)/nearThresholdDepth));
				else
					d.x					= step(d.x,nearThresholdDepth)+d.x;
				if(depthInterpretationStruct.reverseDepth)
				{
					farthest_nearest.yw=max(farthest_nearest.yw,d);
					farthest_nearest.xz=min(farthest_nearest.xz,d);
				}
				else
				{
					farthest_nearest.yw=min(farthest_nearest.yw,d);
					farthest_nearest.xz=max(farthest_nearest.xz,d);
				}

			}
		}
	}
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
		farthest_nearest=saturate(farthest_nearest);
	}
	return farthest_nearest;
}

vec4 Samescale_MSAA(TEXTURE2DMS_FLOAT4 sourceMSDepthTexture
	,int2 source_dims
	,int2 source_offset
	,int2 pos
	,DepthIntepretationStruct depthInterpretationStruct
	,float nearThresholdDepth)
{
	int2 pos2		=pos;
	int2 max_pos	=source_dims-int2(1,1);
	int2 hires_pos	=int2(min(pos2.x,max_pos.x),min(pos2.y,max_pos.y));
	hires_pos		+=source_offset;
	vec4 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec4(1.0,0.0,1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec4(0.0,1.0,0.0,1.0);
	}
	int k=0;
	{
		vec2 d			=TEXTURE_LOAD_MSAA(sourceMSDepthTexture,hires_pos,k).xx;
		if(depthInterpretationStruct.reverseDepth)
			d.x					= lerp(d.x,0.0,saturate(0.01*(d.x-nearThresholdDepth)/nearThresholdDepth));
		else
			d.x					= step(d.x,nearThresholdDepth)+d.x;
		if(depthInterpretationStruct.reverseDepth)
		{
			farthest_nearest.yw=max(farthest_nearest.yw,d);
			farthest_nearest.xz=min(farthest_nearest.xz,d);
		}
		else
		{
			farthest_nearest.yw=min(farthest_nearest.yw,d);
			farthest_nearest.xz=max(farthest_nearest.xz,d);
		}
	}
	return farthest_nearest;
}

vec4 Samescale(Texture2D sourceDepthTexture
	,int2 source_dims
	,int2 source_offset
	,int2 pos
	,DepthIntepretationStruct depthInterpretationStruct
	,bool split_view
	,float nearThresholdDepth)
{
	int2 max_pos	=source_dims-int2(1,1);
	int2 pos1		=int2(min(pos.x,max_pos.x),min(pos.y,max_pos.y));
	pos1			+=source_offset;
	vec4 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
		farthest_nearest		=vec4(1.0,0.0,1.0,0.0);
	else
		farthest_nearest		=vec4(0.0,1.0,0.0,1.0);
	
	vec2 d			=TEXTURE_LOAD(sourceDepthTexture,pos1).xx;
	if(depthInterpretationStruct.reverseDepth)
		d.x					= lerp(d.x,0.0,saturate(0.01*(d.x-nearThresholdDepth)/nearThresholdDepth));
	else
		d.x					= step(d.x,nearThresholdDepth)+d.x;
	
	farthest_nearest.yw=d;
	farthest_nearest.xz=d;
	
	return farthest_nearest;
}

vec4 DownscaleStochastic(Texture2D previousTexture,Texture2D sourceDepthTexture,vec2 sourceTexCoords, vec2 texelRange
	, DepthIntepretationStruct depthInterpretationStruct, float nearThresholdDepth,vec2 stochasticOffset,uint2 scale)
{
	// Stochastic offset goes from -1 to +1
	vec2 texCoords				=sourceTexCoords+texelRange*stochasticOffset/vec2(scale);
	vec4 fn;
	
	if(depthInterpretationStruct.reverseDepth)
		fn				=vec4(1.0,0.0,1.0,0.0);
	else														
		fn				=vec4(0.0,1.0,0.0,1.0);
	vec4 thr			=vec4(nearThresholdDepth, nearThresholdDepth, nearThresholdDepth, nearThresholdDepth);
	for(int i=1;i<scale.x;i+=2)
	{
		float x=(0.5+float(i-scale.x/2.0))/float(scale.x);
		for(int j=1;j<scale.y;j+=2)
		{
			float y		=(0.5+float(j-scale.y/2.0))/float(scale.y);
			vec2 offs	=texelRange*vec2(x,y);
			vec2 texc	=texCoords+2.0*offs;
			vec2 d		=texture_clamp_lod(sourceDepthTexture,texc,0).xx;
			if(depthInterpretationStruct.reverseDepth)
				d.x					= step(d.x,thr)*d.x;
			else
				d.x					= step(d.x,thr)+d.x;
			if(depthInterpretationStruct.reverseDepth)
			{
				fn.yw=max(fn.yw,d);
				fn.xz=min(fn.xz,d);
			}
			else
			{
				fn.yw=min(fn.yw,d);
				fn.xz=max(fn.xz,d);
			}
		}
	}
	return fn;
}

vec4 HalfscaleInitial(Texture2D sourceDepthTexture,int2 source_dims,int2 max_dims
	, uint2 source_offset, int2 cornerOffset, int2 pos
	, DepthIntepretationStruct depthInterpretationStruct
	, bool split_view
	, float nearThresholdDepth)
{
	int2 pos0			=int2(pos * 2);

	int2 pos1			=int2(pos0)-int2(cornerOffset);
//	pos1				=pos1max_dims;
	int2 max_pos		=int2(source_dims)-int2(3,3);
	int2 min_pos		=int2(1,1);
	int2 pos2			=pos1+int2(source_offset);
	pos2				=(pos2+max_dims)%max_dims;
	pos2				=int2(max(min_pos.x,min(pos2.x,max_pos.x)),max(min_pos.y,min(pos2.y,max_pos.y)));
	
	vec2 farthest_nearest,fn0;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest		=vec2(1.0,0.0);
		fn0						=vec2(1.0,0.0);
	}
	else
	{
		farthest_nearest		=vec2(0.0,1.0);
		fn0						=vec2(0.0,1.0);
	}
	float d1				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(1,1)).x;
	float d2				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(0,1)).x;
	float d3				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(1,0)).x;
	float d4				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(0,0)).x;
	vec4 d					=vec4(d1,d2,d3,d4);
	vec4 d0					=d;
	vec4 thr				=vec4(nearThresholdDepth, nearThresholdDepth, nearThresholdDepth, nearThresholdDepth);
	if(depthInterpretationStruct.reverseDepth)
		d					= step(d,thr)*d;
	else
		d					= step(d,thr)+d;
	vec2 dmin2				=min(d.xy,d.zw);
	float dmin				=min(dmin2.x,dmin2.y);
	vec2 dmax2				=max(d.xy,d.zw);
	float dmax				=max(dmax2.x,dmax2.y);
	vec2 dmin20				=min(d0.xy,d0.zw);
	float dmin0				=min(dmin20.x,dmin20.y);
	vec2 dmax20				=max(d0.xy,d0.zw);
	float dmax0				=max(dmax20.x,dmax20.y);
	if(split_view)
	{
		pos2.x			+=source_dims.x;
		d1				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(1,1)).x;
		d2				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(0,1)).x;
		d3				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(1,0)).x;
		d4				=TEXTURE_LOAD(sourceDepthTexture,pos2+int2(0,0)).x;
		d				=vec4(d1,d2,d3,d4);
		d0				=d;
		if(depthInterpretationStruct.reverseDepth)
			d			= step(d, thr)*d;
		else
			d			= step(thr,d)+d;
		dmin2			=min(d.xy,d.zw);
		dmin			=min(dmin,min(dmin2.x,dmin2.y));
		dmax2			=max(d.xy,d.zw);
		dmax			=max(dmax,max(dmax2.x,dmax2.y));
		dmin20			=min(d0.xy,d0.zw);
		dmin0			=min(dmin20.x,dmin20.y);
		dmax20			=max(d0.xy,d0.zw);
		dmax0			=max(dmax20.x,dmax20.y);
	}
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest.y=max(farthest_nearest.y,dmax);
		farthest_nearest.x=min(farthest_nearest.x,dmin);
		fn0.y=max(fn0.y,dmax0);
		fn0.x=min(fn0.x,dmin0);
	}
	else
	{
		farthest_nearest.y=min(farthest_nearest.y,dmin);
		farthest_nearest.x=max(farthest_nearest.x,dmax);
		fn0.y=min(fn0.y,dmin0);
		fn0.x=max(fn0.x,dmax0);
	}
	vec4 res=vec4(farthest_nearest,fn0);
	return res;
}

vec4 Halfscale(Texture2D sourceDepthTexture, uint2 source_dims, uint2 source_offset, int2 cornerOffset, int2 pos, DepthIntepretationStruct depthInterpretationStruct, float nearThresholdDepth)
{
	int2 pos0			=int2(pos * 2);

	int2 pos1			=int2(pos0)-int2(cornerOffset);

	int2 max_pos		=int2(source_dims)-int2(7,7);
	int2 min_pos		=int2(5,2);
	pos1				+=int2(source_offset);
	int2 pos2			=int2(max(min_pos.x,min(pos1.x,max_pos.x)),max(min_pos.y,min(pos1.y,max_pos.y)));
	vec4 farthest_nearest;
	if(depthInterpretationStruct.reverseDepth)
	{
		farthest_nearest	=vec4(1.0,0.0,1.0,0.0);
	}
	else
	{
		farthest_nearest	=vec4(0.0,1.0,0.0,1.0);
	}
	for(int i=0;i<4;i++)
	{
		int2 pos3			=pos2+int2(i*2-2,0);
		vec4 d1				=TEXTURE_LOAD(sourceDepthTexture,pos3+int2(0,-1));
		vec4 d2				=TEXTURE_LOAD(sourceDepthTexture,pos3+int2(0,0));
		vec4 d3				=TEXTURE_LOAD(sourceDepthTexture,pos3+int2(0,1));
		vec4 d4				=TEXTURE_LOAD(sourceDepthTexture,pos3+int2(0,2));
		vec4 f				=vec4(d1.x,d2.x,d3.x,d4.x);
		vec4 n				=vec4(d1.y,d2.y,d3.y,d4.y);
		vec4 f0				=vec4(d1.z,d2.z,d3.z,d4.z);
		vec4 n0				=vec4(d1.w,d2.w,d3.w,d4.w);
		vec4 dmin2,dmax2;
		if(depthInterpretationStruct.reverseDepth)
		{
			dmin2			=min(vec4(f.xy,f0.xy),vec4(f.zw,f0.zw));
			dmax2			=max(vec4(n.xy,n0.xy),vec4(n.zw,n0.zw));
		}
		else
		{
			dmin2			=min(vec4(n.xy,n0.xy),vec4(n.zw,n0.zw));
			dmax2			=max(vec4(f.xy,f0.xy),vec4(f.zw,f0.zw));
		}

		vec2 dmin			=min(dmin2.xz,dmin2.yw);
		vec2 dmax			=max(dmax2.xz,dmax2.yw);
		if(depthInterpretationStruct.reverseDepth)
		{
			farthest_nearest.yw	=max(farthest_nearest.yw,dmax);
			farthest_nearest.xz	=min(farthest_nearest.xz,dmin);
		}
		else
		{
			farthest_nearest.yw	=min(farthest_nearest.yw,dmin);
			farthest_nearest.xz	=max(farthest_nearest.xz,dmax);
		}
	}
	if(depthInterpretationStruct.reverseDepth)
	{
	}
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		if(depthInterpretationStruct.reverseDepth)
		{

		}
		else
		{
			// Force edge at far clip.
			farthest_nearest.xz	=min(farthest_nearest.xz,vec2(1.0,1.0));
			farthest_nearest.yw	=min(farthest_nearest.yw,vec2(1.0,1.0));
		}
		farthest_nearest=saturate(farthest_nearest);
	}
	else
	{
		//if(depthInterpretationStruct.reverseDepth)
		//	farthest_nearest.y = 1.0;
		//else
		//	farthest_nearest.y = 0.0;
	}
	return farthest_nearest;
}
#endif