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

void DownscaleDepthFarNear(Texture2DMS<float4> sourceMSDepthTexture,RWTexture2D<float4> target2DTexture,uint3 pos,vec2 scale,vec3 depthToLinFadeDistParams)
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
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			for(int k=0;k<numberOfSamples;k++)
			{
				uint2 hires_pos		=pos2+uint2(i,j);
				float d				=sourceMSDepthTexture.Load(hires_pos,k).x;
				if(d>nearest_depth)
					nearest_depth	=d;
				if(d<farthest_depth)
					farthest_depth	=d;
			}
		}
	}
	float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
	float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
	float edge	=f-n;
	edge		=1.0;//step(0.0002,edge);
	target2DTexture[pos.xy]	=vec4(farthest_depth,nearest_depth,edge,0.0);
}


void DownscaleDepthFarNear2(Texture2D<float4> sourceDepthTexture,RWTexture2D<float4> target2DTexture,uint3 pos,vec2 scale,vec3 depthToLinFadeDistParams)
{
	uint2 source_dims;
	sourceDepthTexture.GetDimensions(source_dims.x,source_dims.y);
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	uint2 pos2					=pos.xy*scale;
#ifdef REVERSE_DEPTH
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
			float d				=sourceDepthTexture[hires_pos].x;
#ifdef REVERSE_DEPTH
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
	float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
	float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
	float edge	=f-n;
	edge		=step(0.002,edge);
	target2DTexture[pos.xy]	=vec4(nearest_depth,farthest_depth,edge,0.0);
}
#endif
