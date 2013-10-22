#include "CppHlsl.hlsl"
#include "states.hlsl"
Texture2DMS<float4> sourceMSDepthTexture SIMUL_TEXTURE_REGISTER(0);
Texture2D<float4> sourceDepthTexture SIMUL_TEXTURE_REGISTER(0);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float a,b;
	uniform vec3 depthToLinFadeDistParams;
	uniform float ccc;
SIMUL_CONSTANT_BUFFER_END

float AdaptDepth(float depth)
{
	return  depthToLinFadeDistParams.x / (depth*depthToLinFadeDistParams.y + depthToLinFadeDistParams.z);
}

[numthreads(8,8,1)]
void CS_DownscaleDepthFarNear(uint3 pos : SV_DispatchThreadID )
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
	float n=AdaptDepth(nearest_depth);
	float f=AdaptDepth(farthest_depth);
	float edge=f-n;
	edge=1.0;//step(0.0002,edge);
	target2DTexture[pos.xy]	=vec4(farthest_depth,nearest_depth,edge,0.0);
}

vec4 PS_ResolveDepth(posTexVertexOutput IN):SV_Target
{
	uint2 source_dims;
	uint numberOfSamples;
	sourceMSDepthTexture.GetDimensions(source_dims.x,source_dims.y,numberOfSamples);
	uint2 hires_pos		=uint2(vec2(source_dims)*IN.texCoords.xy);
	return sourceMSDepthTexture.Load(hires_pos,0).x;
}

technique11 downscale_depth_far_near
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear()));
    }
}

technique11 resolve_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_ResolveDepth()));
    }
}