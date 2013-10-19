#include "CppHlsl.hlsl"
#include "states.hlsl"
Texture2DMS sourceMSDepthTexture SIMUL_TEXTURE_REGISTER(0);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float a,b;
	uniform vec3 depthToLinFadeDistParams;
	uniform float ccc;
SIMUL_CONSTANT_BUFFER_END

[numthreads(8,8,1)]
void CS_DownscaleDepthNear(uint3 pos : SV_DispatchThreadID )
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
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			for(int k=0;k<numberOfSamples;k++)
			{
				uint2 hires_pos		=pos2+uint2(i,j);
				float d				=sourceDepthTexture[hires_pos].x;
				if(d>nearest_depth)
					nearest_depth	=d;
			}
		}
	}
	target2DTexture[pos.xy]	=nearest_depth;
}
[numthreads(8,8,1)]
void CS_DownscaleDepthFar(uint3 pos : SV_DispatchThreadID )
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
	float farthest_depth		=1.0;
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			for(int k=0;k<numberOfSamples;k++)
			{
				uint2 hires_pos		=pos2+uint2(i,j);
				float d				=sourceDepthTexture[hires_pos].x;
				if(d<farthest_depth)
					farthest_depth	=d;
			}
		}
	}
	target2DTexture[pos.xy]	=farthest_depth;
}
float AdaptDepth(float depth)
{
	return  depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);;
}
[numthreads(8,8,1)]
void CS_FilterLowresDepth(uint3 pos : SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	float farthest_depth		=1.0;
	float blended_depth		=0.0;
	float dx=0;
	float dy=0;
	for(int i=-1;i<2;i++)
	{
		for(int j=-1;j<2;j++)
		{
			uint2 pos2		=pos.xy+uint2(i,j);
			float d			=sourceDepthTexture[pos2].x;
			if(d<farthest_depth)
				farthest_depth	=d;
			blended_depth+=d;
		}
		dx	+=abs(AdaptDepth(sourceDepthTexture[pos.xy+uint2(1,i)].x)-AdaptDepth(sourceDepthTexture[pos.xy+uint2(-1,i)].x));
		dy	+=abs(AdaptDepth(sourceDepthTexture[pos.xy+uint2(i,1)].x)-AdaptDepth(sourceDepthTexture[pos.xy+uint2(i,-1)].x));
	}
	blended_depth/=9.0;
	target2DTexture[pos.xy]	=vec4(sourceDepthTexture[pos.xy].x,dx,dy,0);
}

vec4 PS_ResolveDepth(posTexVertexOutput IN):SV_Target
{
	return texture_clamp_lod(sourceDepthTexture,IN.texCoords,0);
}

technique11 downscale_depth_near
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepth()));
    }
}

technique11 downscale_depth_far
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFar()));
    }
}

technique11 filter_lowres_depth
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_FilterLowresDepthNear()));
    }
}

technique11 resolve_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_ResolveDepth()));
    }
}