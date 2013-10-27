#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

float AdaptDepth(float depth,vec3 depthToLinFadeDistParams)
{
	return  depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
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
	float n=AdaptDepth(nearest_depth,depthToLinFadeDistParams);
	float f=AdaptDepth(farthest_depth,depthToLinFadeDistParams);
	float edge=f-n;
	edge=1.0;//step(0.0002,edge);
	target2DTexture[pos.xy]	=vec4(farthest_depth,nearest_depth,edge,0.0);
}
// Filter the texture, but bias the result towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture,Texture2D depthTexture,vec2 texc,vec4 depthMask,vec3 depthToLinFadeDistParams,float d)
{
	uint width,height;
	imageTexture.GetDimensions(width,height);
	vec2 texc_unit	=texc*vec2(width,height)-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	vec2 xy			=frac(texc_unit);
	uint2 i11		=idx;
	uint2 i21		=idx+uint2(1,0);
	uint2 i12		=idx+uint2(0,1);
	uint2 i22		=idx+uint2(1,1);
	// x = right, y = up, z = left, w = down
	vec4 f11		=imageTexture[i11];
	vec4 f21		=imageTexture[i21];
	vec4 f12		=imageTexture[i12];
	vec4 f22		=imageTexture[i22];

	float d11		=depthToLinearDistance(dot(depthTexture[i11],depthMask),depthToLinFadeDistParams);
	float d21		=depthToLinearDistance(dot(depthTexture[i21],depthMask),depthToLinFadeDistParams);
	float d12		=depthToLinearDistance(dot(depthTexture[i12],depthMask),depthToLinFadeDistParams);
	float d22		=depthToLinearDistance(dot(depthTexture[i22],depthMask),depthToLinFadeDistParams);

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

#endif
