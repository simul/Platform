#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

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
	for(int k=0;k<numberOfSamples;k++)
	{
		d+=sourceTextureMS.Load(pos,k);
	}
	d/=float(numberOfSamples);
	targetTexture[pos.xy]	=d;
}

// Find nearest and furthest depths in MSAA texture.
// sourceDepthTexture, sourceMSDepthTexture, and targetTexture are ALL the SAME SIZE.
vec4 MakeDepthFarNear(Texture2D<float4> sourceDepthTexture,Texture2DMS<float4> sourceMSDepthTexture,uint numberOfSamples,uint2 pos,vec3 depthToLinFadeDistParams)
{
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(int k=0;k<numberOfSamples;k++)
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
		edge		=step(0.002,edge);
	}
	return vec4(farthest_depth,nearest_depth,edge,0.0);
}

void DownscaleDepthFarNear_MSAA(Texture2DMS<float4> sourceMSDepthTexture,RWTexture2D<float4> target2DTexture,uint3 pos,vec2 scale,vec3 depthToLinFadeDistParams)
{
	uint2 source_dims;
	uint numberOfSamples;
	sourceMSDepthTexture.GetDimensions(source_dims.x,source_dims.y,numberOfSamples);
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	uint2 pos2					=pos.xy*scale;
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			for(int k=0;k<numberOfSamples;k++)
			{
				uint2 hires_pos		=pos2+uint2(i,j);
				float d				=sourceMSDepthTexture.Load(hires_pos,k).x;
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
		}
	}
	float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
	float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
	float edge	=f-n;
	edge		=step(0.002,edge);
	target2DTexture[pos.xy]	=vec4(farthest_depth,nearest_depth,edge,0.0);
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

void DownscaleDepthFarNear2(Texture2D<float4> sourceDepthTexture,RWTexture2D<float4> target2DTexture,uint2 source_dims,uint3 pos,uint2 scale,vec3 depthToLinFadeDistParams)
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	uint2 pos2					=pos.xy*scale;
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
				continue;
			vec4 d				=sourceDepthTexture[hires_pos];
#if REVERSE_DEPTH==1
			if(d.y>nearest_depth)
				nearest_depth	=d.y;
			if(d.x<farthest_depth)
				farthest_depth	=d.x;
#else
			if(d.y<nearest_depth)
				nearest_depth	=d.y;
			if(d.x>farthest_depth)
				farthest_depth	=d.x;
#endif
		}
	}
	float edge=0.0;
	if(nearest_depth!=farthest_depth)
	{
		float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
		float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
		edge		=f-n;
		edge		=step(0.002,edge);
	}
	target2DTexture[pos.xy]	=vec4(farthest_depth,nearest_depth,edge,0);
}

#ifndef GLSL


// Filter the texture, but bias the result towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture,Texture2D fallbackTexture,Texture2D depthTexture,vec2 lowResDims,vec2 texc,vec4 depthMask,vec3 depthToLinFadeDistParams,float d,bool do_fallback)
{
	vec2 texc_unit	=texc*lowResDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	vec2 xy			=frac(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,lowResDims.x-1);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,lowResDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);
	// x = right, y = up, z = left, w = down
	vec4 f11		=imageTexture[i11];
	vec4 f21		=imageTexture[i21];
	vec4 f12		=imageTexture[i12];
	vec4 f22		=imageTexture[i22];
	vec4 de11		=depthTexture[i11];
	vec4 de21		=depthTexture[i21];
	vec4 de12		=depthTexture[i12];
	vec4 de22		=depthTexture[i22];

	if(do_fallback)
	{
		f11			*=de11.z;
		f21			*=de21.z;
		f12			*=de12.z;
		f22			*=de22.z;
		f11			+=fallbackTexture[i11]*(1.0-de11.z);
		f21			+=fallbackTexture[i21]*(1.0-de21.z);
		f12			+=fallbackTexture[i12]*(1.0-de12.z);
		f22			+=fallbackTexture[i22]*(1.0-de22.z);
	}
	float d11		=depthToLinearDistance(dot(de11,depthMask),depthToLinFadeDistParams);
	float d21		=depthToLinearDistance(dot(de21,depthMask),depthToLinFadeDistParams);
	float d12		=depthToLinearDistance(dot(de12,depthMask),depthToLinFadeDistParams);
	float d22		=depthToLinearDistance(dot(de22,depthMask),depthToLinFadeDistParams);

	// But now we modify these values:
	float D1		=saturate((d-d11)/(d21-d11));
	float delta1	=abs(d21-d11);			
	f11				=lerp(f11,f21,delta1*D1);
	f21				=lerp(f21,f11,delta1*(1-D1));
	float D2		=saturate((d-d12)/(d22-d12));
	float delta2	=abs(d22-d12);			
	f12				=lerp(f12,f22,delta2*D2);
	f22				=lerp(f22,f12,delta2*(1-D2));

	vec4 f1			=lerp(f11,f21,xy.x);
	vec4 f2			=lerp(f12,f22,xy.x);
	
	float d1		=lerp(d11,d21,xy.x);
	float d2		=lerp(d12,d22,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);

	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);

	return f;
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
							,vec3 depthToLinFadeDistParams
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
	int2 hires_depth_pos2;
	int numSamples;
	if(use_msaa)
		GetMSAACoordinates(depthTextureMS,depth_texc,hires_depth_pos2,numSamples);
	else
	{
		GetCoordinates(depthTexture,depth_texc,hires_depth_pos2);
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
				hiresDepth=depthTextureMS.Load(hires_depth_pos2,i).x;
			else
				hiresDepth=depthTexture[hires_depth_pos2].x;
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
				hiresDepth	=depthTextureMS.Load(hires_depth_pos2,j).x;
			else
				hiresDepth	=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			cloudNear		=depthDependentFilteredImage(lowResNearTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(0,1.0,0,0),depthToLinFadeDistParams,trueDist,true);
			cloudFar		=depthDependentFilteredImage(lowResFarTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist,false);
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
		result				/=float(numSamples);
		hiResInterp			/=float(numSamples);
		if(!use_msaa)
		{
			result.rgb		+=insc_far.rgb*result.a;
		}
	}
	else
	{
		float hiresDepth=0.0;
		// Just use the zero MSAA sample if we're not at an edge:
		if(use_msaa)
			hiresDepth			=depthTextureMS.Load(hires_depth_pos2,0).x;
		else
			hiresDepth			=depthTexture[hires_depth_pos2].x;
		float trueDist		=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		result				=depthDependentFilteredImage(lowResFarTexture,lowResFarTexture,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist,false);
		result.rgb			+=insc_far.rgb*result.a;
		//result.r=1;
	}
    return result;
}

#endif
#endif
