#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL
#if REVERSE_DEPTH==1
#define EDGE_FACTOR (0.0002)
#else
#define EDGE_FACTOR (0.0005)
#endif
struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};

void Resolve(Texture2DMS<float4> sourceTextureMS,RWTexture2D<float4> targetTexture,uint2 pos)
{
	uint2 source_dims;
	uint numberOfSamples;
	sourceTextureMS.GetDimensions(source_dims.x,source_dims.y,numberOfSamples);
	uint2 dims;
	targetTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	vec4 d=vec4(0,0,0,0);
	for(uint k=0;k<numberOfSamples;k++)
	{
		d+=sourceTextureMS.Load(pos,k);
	}
	d/=float(numberOfSamples);
	targetTexture[pos.xy]	=d;
}

// Find nearest and furthest depths in MSAA texture.
// sourceDepthTexture, sourceMSDepthTexture, and targetTexture are ALL the SAME SIZE.
vec4 MakeDepthFarNear(Texture2D<float4> sourceDepthTexture,Texture2DMS<float4> sourceMSDepthTexture,uint numberOfSamples,uint2 pos,vec4 depthToLinFadeDistParams)
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


vec4 DownscaleDepthFarNear2(Texture2D<float4> sourceDepthTexture,uint2 source_dims,uint2 source_offset,int2 pos,vec4 depthToLinFadeDistParams)
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
	pos2=max(int2(1,1),min(pos2,source_dims-uint2(3,3)));
	pos2+=source_offset;
	for(int i=0;i<4;i++)
	{
#if 1
		int2 hires_pos		=pos2+int2(i-1,-1);
		vec4 u				=vec4(	sourceDepthTexture[int2(hires_pos.x,hires_pos.y+0)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+1)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+2)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+3)].x);
		ExtendDepths(farthest_nearest,u.xyzw);
#else
		int2 hires_pos		=pos2+int2(i-1,-1);
		vec4 u				=vec4(	sourceDepthTexture[int2(hires_pos.x,hires_pos.y+0)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+1)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+2)].x
									,sourceDepthTexture[int2(hires_pos.x,hires_pos.y+3)].x);
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
#endif
	}
	float edge=0.0;
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthToLinFadeDistParams);
	//fn =saturate(0.07 / (farthest_nearest.xy*depthToLinFadeDistParams.y ));
		edge	=abs(fn.x-fn.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	vec4 res=vec4(farthest_nearest,edge,0);
	return res;
}

vec4 DownscaleDepthFarNear_MSAA4(Texture2DMS<float4> sourceMSDepthTexture,uint2 source_dims,uint2 source_offset,int2 pos,vec2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos2					=source_offset+pos*scale;
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
	if(farthest_nearest.x!=farthest_nearest.y)
	{
		vec2 fn	=depthToLinearDistance(farthest_nearest.xy,depthToLinFadeDistParams);
		edge	=abs(fn.x-fn.y);
		//edge	=abs(farthest_nearest.x-farthest_nearest.y);
		edge	=step(EDGE_FACTOR,edge);
	}
	return		vec4(farthest_nearest,edge,0.0);
}

vec4 DownscaleDepthFarNear_MSAA(Texture2DMS<float4> sourceMSDepthTexture,uint2 source_dims,uint2 source_offset,int numberOfSamples,int2 pos,vec2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	int2 pos2					=source_offset+pos*scale;
#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	for(int i=0;i<2;i++)
	{
		for(int j=0;j<2;j++)
		{
			int2 hires_pos		=pos2+uint2(i,j);
			//if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
			//	continue;
			for(int k=0;k<numberOfSamples;k++)
			{
				float d				=sourceMSDepthTexture.Load(hires_pos,k).x;
#if REVERSE_DEPTH==1
				if(d>farthest_nearest.y)
					farthest_nearest.y	=d;
				if(d<farthest_nearest.x)
					farthest_nearest.x	=d;
#else
				if(d<farthest_nearest.y)
					farthest_nearest.y	=d;
				if(d>farthest_nearest.x)
					farthest_nearest.x	=d;
#endif
			}
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

vec4 DownscaleFarNearEdge(Texture2D<float4> sourceDepthTexture,uint2 source_dims,uint2 pos,uint2 scale,vec4 depthToLinFadeDistParams)
{
	// pos is the texel position in the target.
	uint2 pos2=pos*scale;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.

#if REVERSE_DEPTH==1
	vec2 farthest_nearest		=vec2(1.0,0.0);
#else
	vec2 farthest_nearest		=vec2(0.0,1.0);
#endif
	vec4 res=vec4(0,0,0,0);
	uint2 maxpos=source_dims.xy-uint2(2,2);
#if 0
	for(uint i=0;i<scale.x;i+=2)
	{
		for(uint j=0;j<scale.y;j+=2)
		{
			uint2 hires_pos		=uint2(pos2.x+i,pos2.y+j);
			// MUST do this in case we go slightly over the edge:
			//hires_pos.xy		=min(hires_pos.xy,maxpos);
			vec2 c11			=sourceDepthTexture[hires_pos			].xy;
			vec2 c12			=sourceDepthTexture[hires_pos+uint2(0,1)].xy;
			vec2 c21			=sourceDepthTexture[hires_pos+uint2(1,0)].xy;
			vec2 c22			=sourceDepthTexture[hires_pos+uint2(1,1)].xy;
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
	for(uint i=0;i<scale.x;i++)
	{
		for(uint j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=uint2(pos2.x+i,pos2.y+j);
			// MUST do this in case we go slightly over the edge:
			hires_pos.xy		=min(hires_pos.xy,maxpos);
		//	if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
		//		continue;
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
#ifndef GLSL

struct LookupQuad4
{
	vec4 _11;
	vec4 _21;
	vec4 _12;
	vec4 _22;
};

void ExtractCompactedLookupQuads(out LookupQuad4 farQ,out LookupQuad4 nearQ,Texture2D<uint4> image,vec2 texc,vec2 texDims)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,texDims.x-1);
	int j2			=max(0,idx.y);
	int j1			=min(idx.y+1,texDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);
	uint4 v11		=image[i11];
	uint4 v21		=image[i21];
	uint4 v12		=image[i12];
	uint4 v22		=image[i22];
	farQ._11=vec4(uint2_to_colour3(v11.xy),1.0);
	farQ._21=vec4(uint2_to_colour3(v21.xy),1.0);
	farQ._12=vec4(uint2_to_colour3(v12.xy),1.0);
	farQ._22=vec4(uint2_to_colour3(v22.xy),1.0);
	nearQ._11=vec4(uint2_to_colour3(v11.zw),1.0);
	nearQ._21=vec4(uint2_to_colour3(v21.zw),1.0);
	nearQ._12=vec4(uint2_to_colour3(v12.zw),1.0);
	nearQ._22=vec4(uint2_to_colour3(v22.zw),1.0);
}
LookupQuad4 GetLookupQuad(Texture2D image,vec2 texc,vec2 texDims)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,texDims.x-1);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,texDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);

	LookupQuad4 q;
	q._11=image[i11];
	q._21=image[i21];
	q._12=image[i12];
	q._22=image[i22];
	return q;
}

LookupQuad4 GetDepthLookupQuad(Texture2D image,vec2 texc,vec2 texDims,vec4 depthToLinFadeDistParams)
{
	LookupQuad4 q	=GetLookupQuad(image,texc,texDims);
	q._11.xy		=depthToLinearDistance(q._11.xy,depthToLinFadeDistParams);
	q._21.xy		=depthToLinearDistance(q._21.xy,depthToLinFadeDistParams);
	q._12.xy		=depthToLinearDistance(q._12.xy,depthToLinFadeDistParams);
	q._22.xy		=depthToLinearDistance(q._22.xy,depthToLinFadeDistParams);
	return q;
}

LookupQuad4 MaskDepth(LookupQuad4 d1,vec2 depthMask)
	{
	LookupQuad4 d2=d1;
	d2._11.x		=dot(d1._11.xy,depthMask.xy);
	d2._21.x		=dot(d1._21.xy,depthMask.xy);
	d2._12.x		=dot(d1._12.xy,depthMask.xy);
	d2._22.x		=dot(d1._22.xy,depthMask.xy);
	return d2;
}

LookupQuad4 LerpZ(LookupQuad4 q,LookupQuad4 f,LookupQuad4 interp)
{
	q._11			=lerp(f._11,q._11,interp._11.z);
	q._21			=lerp(f._21,q._21,interp._21.z);
	q._12			=lerp(f._12,q._12,interp._12.z);
	q._22			=lerp(f._22,q._22,interp._22.z);
	return q;
}

vec4 depthFilteredTexture(	LookupQuad4 image
							,LookupQuad4 dist
							,vec2 xy
							,float d)
{
	float D1		=saturate((d-dist._11.x)/(dist._21.x-dist._11.x));
	float delta1	=abs(dist._21.x-dist._11.x);			
	image._11		=lerp(image._11,image._21,delta1*D1);
	image._21		=lerp(image._21,image._11,delta1*(1-D1));
	float D2		=saturate((d-dist._12.x)/(dist._22.x-dist._12.x));
	float delta2	=abs(dist._22.x-dist._12.x);			
	image._12		=lerp(image._12,image._22,delta2*D2);
	image._22		=lerp(image._22,image._12,delta2*(1-D2));

	vec4 f1			=lerp(image._11,image._21,xy.x);
	vec4 f2			=lerp(image._12,image._22,xy.x);
	
	float d1		=lerp(dist._11.x,dist._21.x,xy.x);
	float d2		=lerp(dist._12.x,dist._22.x,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);
#if 1
	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);
#else
	vec4 F1			=lerp(f1,f2,delta*D);
	vec4 F2			=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(F1,F2,xy.y);
#endif
	return f;
}

// Filter the texture, but bias the result towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture
								 ,Texture2D fallbackTexture
								 ,Texture2D depthTexture
								 ,vec2 texDims
								 ,vec2 texc
								 ,vec2 depthMask
								 ,vec4 depthToLinFadeDistParams
								 ,float d
								 ,bool do_fallback)
{
#if 0
	return texture_clamp_lod(imageTexture,texc,0);
#else
	LookupQuad4 image		=GetLookupQuad(imageTexture		,texc,texDims);
	LookupQuad4 fallback	=GetLookupQuad(fallbackTexture	,texc,texDims);
	LookupQuad4 dist		=GetDepthLookupQuad(depthTexture,texc,texDims,depthToLinFadeDistParams);
	dist					=MaskDepth(dist,depthMask.xy);
	vec2 texc_unit			=texc*texDims-vec2(.5,.5);
	vec2 xy					=frac(texc_unit);
	return depthFilteredTexture(image
								,dist
								,xy
								,d);
#endif
}

// Blending (not just clouds but any low-resolution alpha-blended volumetric image) into a hi-res target.
// Requires a near and a far image, a low-res depth texture with far (true) depth in the x, near depth in the y and edge in the z;
// a hi-res MSAA true depth texture.
vec4 NearFarDepthCloudBlend(vec2 texCoords
							,Texture2D lowResFarTexture
							,Texture2D lowResNearTexture
							,Texture2D hiResDepthTexture
							,Texture2D lowResDepthTexture
							,Texture2D<vec4> depthTexture
							,Texture2DMS<vec4> depthTextureMS
							,vec4 viewportToTexRegionScaleBias
							,vec4 depthToLinFadeDistParams
							,vec4 hiResToLowResTransformXYWH
							,Texture2D farInscatterTexture
							,Texture2D nearInscatterTexture
							,bool use_msaa)
{
	// texCoords.y is positive DOWNwards
	uint width,height;
	lowResNearTexture.GetDimensions(width,height);
	vec2 lowResDims	=vec2(width,height);

	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 fullres_depth_pos2;
	int numSamples;
	if(use_msaa)
		GetMSAACoordinates(depthTextureMS,depth_texc,fullres_depth_pos2,numSamples);
	else
	{
		GetCoordinates(depthTexture,depth_texc,fullres_depth_pos2);
		numSamples=1;
	}
	vec2 lowResTexCoords		=hiResToLowResTransformXYWH.xy+hiResToLowResTransformXYWH.zw*texCoords;
	// First get the values that don't vary with MSAA sample:
	vec4 cloudFar;
	vec4 cloudNear				=vec4(0,0,0,1.0);
	vec4 lowres					=texture_clamp_lod(lowResDepthTexture,lowResTexCoords,0);
	vec4 hires					=texture_clamp_lod(hiResDepthTexture,texCoords,0);
	float lowres_edge			=lowres.z;
	float hires_edge			=hires.z;
	vec4 result					=vec4(0,0,0,0);
	vec2 nearFarDistLowRes		=depthToLinearDistance(lowres.yx,depthToLinFadeDistParams);
	vec4 insc					=vec4(0,0,0,0);
	vec4 insc_far				=texture_nearest_lod(farInscatterTexture,texCoords,0);
	vec4 insc_near				=texture_nearest_lod(nearInscatterTexture,texCoords,0);
	if(lowres_edge>0.0)
	{
		vec2 nearFarDistHiRes	=vec2(1.0,0.0);
		for(int i=0;i<numSamples;i++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth=depthTextureMS.Load(fullres_depth_pos2,i).x;
			else
				hiresDepth=depthTexture[fullres_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		// Find the near and far depths at full resolution.
			if(trueDist<nearFarDistHiRes.x)
				nearFarDistHiRes.x=trueDist;
			if(trueDist>nearFarDistHiRes.y)
				nearFarDistHiRes.y=trueDist;
		}
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		float hiResInterp	=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth	=depthTextureMS.Load(fullres_depth_pos2,j).x;
			else
				hiresDepth	=depthTexture[fullres_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			cloudNear		=depthDependentFilteredImage(lowResNearTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec2(0,1.0),depthToLinFadeDistParams,trueDist,true);
			cloudFar		=depthDependentFilteredImage(lowResFarTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec2(1.0,0),depthToLinFadeDistParams,trueDist,false);
			float interp	=saturate((nearFarDistLowRes.y-trueDist)/abs(nearFarDistLowRes.y-nearFarDistLowRes.x));
			vec4 add		=lerp(cloudFar,cloudNear,lowres_edge*interp);
			result			+=add;
		
			if(use_msaa)
			{
				hiResInterp		=hires_edge*saturate((nearFarDistHiRes.y-trueDist)/(nearFarDistHiRes.y-nearFarDistHiRes.x));
				insc			=lerp(insc_far,insc_near,hiResInterp);
				result.rgb		+=insc.rgb*add.a;
			}
		}
		// atmospherics: we simply interpolate.
		if(use_msaa)
		{
			result				/=float(numSamples);
			hiResInterp			/=float(numSamples);
		}
		else
		{
			result.rgb		+=insc_far.rgb*result.a;
		}
		//result=vec4(1.0,0,0,1.0);
	}
	else
	{
		float hiresDepth=0.0;
		// Just use the zero MSAA sample if we're not at an edge:
		if(use_msaa)
			hiresDepth			=depthTextureMS.Load(fullres_depth_pos2,0).x;
		else
			hiresDepth			=depthTexture[fullres_depth_pos2].x;
		float trueDist		=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		result				=depthDependentFilteredImage(lowResFarTexture,lowResFarTexture,lowResDepthTexture,lowResDims,lowResTexCoords,vec2(1.0,0),depthToLinFadeDistParams,trueDist,false);
		result.rgb			+=insc_far.rgb*result.a;
		//result.r=1;
	}
    return result;
}
vec3 nearFarDepthFilter(LookupQuad4 depth_Q,LookupQuad4 dist_Q,vec2 texDims,vec2 texc,vec2 dist)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	vec2 xy			=frac(texc_unit);
	// x = right, y = up, z = left, w = down
	vec3 f11		=depth_Q._11.xyz;
	vec3 f21		=depth_Q._21.xyz;
	vec3 f12		=depth_Q._12.xyz;
	vec3 f22		=depth_Q._22.xyz;

	vec2 d11		=dist_Q._11.xy;
	vec2 d21		=dist_Q._21.xy;
	vec2 d12		=dist_Q._12.xy;
	vec2 d22		=dist_Q._22.xy;

	// But now we modify these values:
	vec2 D1			=saturate((dist-d11)/(d21-d11));
	vec2 delta1		=abs(d21-d11);			
	vec2 D2			=saturate((dist-d12)/(d22-d12));
	vec2 delta2		=abs(d22-d12);
	
	vec2 d1			=lerp(d11,d21,xy.x);
	vec2 d2			=lerp(d12,d22,xy.x);
	
	vec2 D			=saturate((dist-d1)/(d2-d1));
	vec2 delta		=abs(d2-d1);
#if 1
	f11.xy				=lerp(f11.xy,f21.xy,delta1*D1);
	f21.xy				=lerp(f21.xy,f11.xy,delta1*(1-D1));
	f12.xy				=lerp(f12.xy,f22.xy,delta2*D2);
	f22.xy				=lerp(f22.xy,f12.xy,delta2*(1-D2));
#endif
	vec3 f1			=lerp(f11,f21,xy.x);
	vec3 f2			=lerp(f12,f22,xy.x);
#if 1
	f1.xy			=lerp(f1.xy,f2.xy,delta*D);
	f2.xy			=lerp(f2.xy,f1.xy,delta*(1.0-D));
#endif
	vec3 f			=lerp(f1,f2,xy.y);

	return f;
}

TwoColourCompositeOutput Composite(vec2 texCoords
				,Texture2D lowResFarTexture
				,Texture2D lowResNearTexture
				,Texture2D hiResDepthTexture
				,uint2 hiResDims
				,Texture2D lowResDepthTexture
				,uint2 lowResDims
				,Texture2D<vec4> depthTexture
				,uint2 fullResDims
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec4 fullResToLowResTransformXYWH
				,vec4 fullResToHighResTransformXYWH
				,Texture2D farInscatterTexture
				,Texture2D nearInscatterTexture
				,Texture2D<uint4> lossTexture)
{
	// texCoords.y is positive DOWNwards
	TwoColourCompositeOutput result;
	vec2 depth_texc				=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);

	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	vec2 lowResTexCoords		=fullResToLowResTransformXYWH.xy+texCoords*fullResToLowResTransformXYWH.zw;
	vec2 hiResTexCoords			=fullResToHighResTransformXYWH.xy+texCoords*fullResToHighResTransformXYWH.zw;
	vec4 lowres_depths			=texture_clamp_lod(lowResDepthTexture	,lowResTexCoords	,0);
	vec4 hires_depths			=texture_clamp_lod(hiResDepthTexture	,hiResTexCoords		,0);
	float lowres_edge			=lowres_depths.z;
	float hires_edge			=hires_depths.z;
	result.add					=vec4(0,0,0,0);
	vec4 insc					=vec4(0,0,0,0);
	float depth					=depthTexture[fullres_depth_pos2].x;
	float dist					=depthToLinearDistance(depth,depthToLinFadeDistParams);
	if(lowres_edge>0.0||hires_edge>0.0)
	{
		LookupQuad4 cloudNear_Q		=GetLookupQuad(lowResNearTexture		,lowResTexCoords,lowResDims);
		LookupQuad4 cloudFar_Q		=GetLookupQuad(lowResFarTexture			,lowResTexCoords,lowResDims);
		LookupQuad4 distFar_Q		=GetDepthLookupQuad(lowResDepthTexture	,lowResTexCoords,lowResDims,depthToLinFadeDistParams);
		LookupQuad4 distNear_Q		=MaskDepth(distFar_Q,vec2(0,1.0));
		vec2 texc_unit				=lowResTexCoords*lowResDims-vec2(.5,.5);
		vec2 xy						=frac(texc_unit);
		vec4 cloudNear				=depthFilteredTexture(	cloudNear_Q
															,distNear_Q
															,xy
															,dist);
		vec4 cloudFar				=depthFilteredTexture(	cloudFar_Q
															,distFar_Q
															,xy
															,dist);
		vec3 nearFarDistLowRes;
		nearFarDistLowRes.xy=depthToLinearDistance(lowres_depths.yx	,depthToLinFadeDistParams);
		nearFarDistLowRes.z=abs(nearFarDistLowRes.y-nearFarDistLowRes.x);
		float interp	=saturate((nearFarDistLowRes.y-dist)/nearFarDistLowRes.z);
		result.add		=lerp(cloudFar,cloudNear,interp);
		{
			LookupQuad4 inscNear_Q		=GetLookupQuad(nearInscatterTexture		,hiResTexCoords,hiResDims);
			LookupQuad4 inscFar_Q		=GetLookupQuad(farInscatterTexture		,hiResTexCoords,hiResDims);
			LookupQuad4 distFar_Q		=GetDepthLookupQuad(hiResDepthTexture	,hiResTexCoords,hiResDims,depthToLinFadeDistParams);
			LookupQuad4 distNear_Q		=MaskDepth(distFar_Q,vec2(0,1.0));
		
			LookupQuad4 lossFar_Q,lossNear_Q;
			ExtractCompactedLookupQuads(lossFar_Q,lossNear_Q,lossTexture,hiResTexCoords,hiResDims);

			vec2 texc_unit				=hiResTexCoords*hiResDims-vec2(.5,.5);
			vec2 xy						=frac(texc_unit);
			vec4 insc_far				=depthFilteredTexture(	inscFar_Q
																,distFar_Q
																,xy
																,dist);
			vec4 insc_near				=depthFilteredTexture(	inscNear_Q
																,distNear_Q
																,xy
																,dist);
			vec3 nearFarDistHiRes;
			nearFarDistHiRes.xy		=depthToLinearDistance(hires_depths.yx	,depthToLinFadeDistParams);
			nearFarDistHiRes.z		=abs(nearFarDistHiRes.y-nearFarDistHiRes.x);
			// Given that we have the near and far depths, 
			// At an edge we will do the interpolation for each MSAA sample.
			float hiResInterp		=saturate((nearFarDistHiRes.y-dist)/nearFarDistHiRes.z);
			insc					=lerp(insc_far,insc_near,hiResInterp);
			//uint2 loss				=image_load(lossTexture,isncTexc_unit).xy;
				// 
			vec4 loss_far			=depthFilteredTexture(	lossFar_Q
															,distFar_Q
															,xy
															,dist);
			vec4 loss_near			=depthFilteredTexture(	lossNear_Q
															,distNear_Q
															,xy
															,dist);
			result.multiply			=lerp(loss_far,loss_near,hiResInterp)*result.add.a;
		}
		//result.add.r=hires_edge;
	}
	else
	{	
		result.add		+=texture_clamp_lod(lowResFarTexture,lowResTexCoords,0);
		insc					=texture_clamp_lod(farInscatterTexture,hiResTexCoords,0);
		vec2 inscTexc_unit		=hiResTexCoords*hiResDims-vec2(.5,.5);
		uint2 loss				=image_load(lossTexture,inscTexc_unit).xy;
		result.multiply			=vec4(uint2_to_colour3(loss.xy),1.0)*result.add.a;
	}
	result.add.rgb				+=insc.rgb*result.add.a;
	result.add.a				=result.multiply.r;
	//	result.add.gb=0;
    return result;
}

TwoColourCompositeOutput Composite_MSAA(vec2 texCoords
				,Texture2D lowResFarTexture
				,Texture2D lowResNearTexture
				,Texture2D hiResDepthTexture
				,uint2 hiResDims
				,Texture2D lowResDepthTexture
				,uint2 lowResDims
				,Texture2DMS<vec4> depthTextureMS
				,int numSamples
				,uint2 fullResDims
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec4 fullResToLowResTransformXYWH
				,vec4 fullResToHighResTransformXYWH
				,Texture2D farInscatterTexture
				,Texture2D nearInscatterTexture
				,Texture2D<uint4> lossTexture)
{
	// texCoords.y is positive DOWNwards
	
	vec2 depth_texc				=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	
	vec2 lowresTexel			=vec2(1.0/lowResDims.x,1.0/lowResDims.y);
	vec2 hiresTexel				=vec2(1.0/hiResDims.x,1.0/hiResDims.y);
	vec2 fullresTexel			=vec2(1.0/fullResDims.x,1.0/fullResDims.y);
	vec2 lowResTexCoords		=fullResToLowResTransformXYWH.xy+texCoords*fullResToLowResTransformXYWH.zw;
	vec2 hiResTexCoords			=fullResToHighResTransformXYWH.xy+texCoords*fullResToHighResTransformXYWH.zw;
	
	//hiResTexCoords				-=0.5*fullresTexel;

	vec4 lowres_depths			=texture_clamp_lod(lowResDepthTexture	,lowResTexCoords	,0);
	vec4 hires_depths			=texture_clamp_lod(hiResDepthTexture	,hiResTexCoords		,0);
	float lowres_edge			=lowres_depths.z;
	float hires_edge			=hires_depths.z;
	TwoColourCompositeOutput result;
	result.add					=vec4(0,0,0,0);
	result.multiply				=vec4(0,0,0,0);
	vec4 insc					=vec4(0,0,0,0);
	if(lowres_edge>0.0||hires_edge>0.0)
	{
		float nearestDepth=0.0;
		float furthestDepth=1.0;
		
		for(int k=0;k<numSamples;k++)
		{
			nearestDepth			=max(nearestDepth,depthTextureMS.Load(fullres_depth_pos2,k).x);
			furthestDepth			=min(furthestDepth,depthTextureMS.Load(fullres_depth_pos2,k).x);
		}
		float nearestDist			=depthToLinearDistance(nearestDepth		,depthToLinFadeDistParams);
		float furthestDist			=depthToLinearDistance(furthestDepth	,depthToLinFadeDistParams);

		vec4 cloudNear				;
		vec4 cloudFar				;

		LookupQuad4 cloudNear_Q		=GetLookupQuad(lowResNearTexture		,lowResTexCoords,lowResDims);
		LookupQuad4 cloudFar_Q		=GetLookupQuad(lowResFarTexture			,lowResTexCoords,lowResDims);
		LookupQuad4 distFar_Q		=GetDepthLookupQuad(lowResDepthTexture	,lowResTexCoords,lowResDims,depthToLinFadeDistParams);
		LookupQuad4 distNear_Q		=MaskDepth(distFar_Q,vec2(0,1.0));
		
		vec2 texc_unit				=lowResTexCoords*lowResDims-vec2(.5,.5);
		vec2 xy						=frac(texc_unit);
#if REVERSE_DEPTH==1
			cloudNear					=depthFilteredTexture(	cloudNear_Q
																,distNear_Q
															,xy
															,nearestDist);
			cloudFar					=depthFilteredTexture(	cloudFar_Q
																,distFar_Q
															,xy
															,furthestDist);
#endif

		vec4 insc_far,insc_near,loss_far,loss_near;
		LookupQuad4 inscNear_Q		=GetLookupQuad(nearInscatterTexture		,hiResTexCoords,hiResDims);
		LookupQuad4 inscFar_Q		=GetLookupQuad(farInscatterTexture		,hiResTexCoords,hiResDims);
		LookupQuad4 inscDistFar_Q	=GetDepthLookupQuad(hiResDepthTexture	,hiResTexCoords,hiResDims,depthToLinFadeDistParams);
		LookupQuad4 inscDistNear_Q	=MaskDepth(inscDistFar_Q,vec2(0,1.0));
		LookupQuad4 lossFar_Q,lossNear_Q;
		ExtractCompactedLookupQuads(lossFar_Q,lossNear_Q,lossTexture,hiResTexCoords,hiResDims);
		vec2 isncTexc_unit			=hiResTexCoords*hiResDims-vec2(.5,.5);
		vec2 inscXy					=frac(isncTexc_unit);
#if REVERSE_DEPTH==1
		insc_far					=depthFilteredTexture(	inscFar_Q
															,inscDistFar_Q
															,inscXy
															,furthestDist);
		insc_near					=depthFilteredTexture(	inscNear_Q
															,inscDistNear_Q
															,inscXy
															,nearestDist);
		loss_far					=depthFilteredTexture(	lossFar_Q
															,distFar_Q
															,xy
															,furthestDist);
		loss_near					=depthFilteredTexture(	lossNear_Q
															,distNear_Q
															,xy
															,nearestDist);
#endif
		vec3 nearFarDistLowRes;
		nearFarDistLowRes.xy=depthToLinearDistance(lowres_depths.yx	,depthToLinFadeDistParams);
		nearFarDistLowRes.z=abs(nearFarDistLowRes.y-nearFarDistLowRes.x);
		vec3 nearFarDistHiRes;
		nearFarDistHiRes.xy=depthToLinearDistance(hires_depths.yx	,depthToLinFadeDistParams);
		nearFarDistHiRes.z=abs(nearFarDistHiRes.y-nearFarDistHiRes.x);
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		float hiResInterp	=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth	=depthTextureMS.Load(fullres_depth_pos2,j).x;
			float trueDist		=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			vec4 add			=vec4(0,0,0,1.0);
			//if(lowres_edge>0.0)
			{
#if REVERSE_DEPTH==0
				cloudNear		=depthFilteredTexture(	cloudNear_Q
														,distNear_Q
														,xy
														,trueDist);
				cloudFar		=depthFilteredTexture(	cloudFar_Q
														,distFar_Q
														,xy
														,trueDist);
#endif
				float interp	=saturate((nearFarDistLowRes.y-trueDist)/nearFarDistLowRes.z);
				add				=lerp(cloudFar,cloudNear,interp);
				result.add		+=add;
			}
			//if(hires_edge>0.0)
			{
				hiResInterp		=saturate((nearFarDistHiRes.y-trueDist)/nearFarDistHiRes.z);
#if REVERSE_DEPTH==0
				insc_far		=depthFilteredTexture(	inscFar_Q
														,inscDistFar_Q
														,inscXy
														,trueDist);
				insc_near		=depthFilteredTexture(	inscNear_Q
														,inscDistNear_Q
														,inscXy
														,trueDist);
				loss_far			=depthFilteredTexture(	lossFar_Q
															,distFar_Q
															,inscXy
															,trueDist);
				loss_near			=depthFilteredTexture(	lossNear_Q
															,distNear_Q
															,inscXy
															,trueDist);
#endif
				insc				=lerp(insc_far,insc_near,hiResInterp);
				vec4 loss			=lerp(loss_far,loss_near,hiResInterp);
				result.add.rgb		+=insc.rgb*add.a;
				result.multiply.rgb	+=loss.rgb*add.a;
			//	result.rgb=hiResInterp;
			}
		}
		// atmospherics: we simply interpolate.
		result.add				/=float(numSamples);
		result.multiply			/=float(numSamples);
		//result.gb=nearFarDistHiRes.xy;
	}
	else
	{
		if(lowres_edge<=0.0)
			result.add			+=texture_clamp_lod(lowResFarTexture,lowResTexCoords,0);
		if(hires_edge<=0.0)
		{	
			vec4 insc			=texture_clamp_lod(farInscatterTexture,hiResTexCoords,0);
			result.add.rgb		+=insc.rgb*result.add.a;
			vec2 inscTexc_unit	=hiResTexCoords*hiResDims-vec2(.5,.5);
			uint2 loss			=image_load(lossTexture,inscTexc_unit).xy;
			result.multiply		=vec4(uint2_to_colour3(loss.xy),1.0)*result.add.a;
		}
	}
	
    return result;
}
#endif
#endif
