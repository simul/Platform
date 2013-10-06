#include "CppHlsl.hlsl"
#include "states.hlsl"
Texture2D<float4> sourceDepthTexture SIMUL_TEXTURE_REGISTER(0);
RWTexture2D<float> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

SIMUL_CONSTANT_BUFFER(MixedResolutionConstants,11)
	uniform uint2 scale;
	uniform float a,b;
SIMUL_CONSTANT_BUFFER_END

[numthreads(8,8,1)]
void CS_DownscaleDepth(uint3 pos : SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	uint2 pos2					=pos.xy*scale;
	float farthest_depth		=1.0;
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			float d				=sourceDepthTexture[hires_pos].x;
			if(d<farthest_depth)
				farthest_depth	=d;
		}
	}
	target2DTexture[pos.xy]	=farthest_depth;
}

vec4 PS_ResolveDepth(posTexVertexOutput IN):SV_Target
{
	return texture_clamp_lod(sourceDepthTexture,IN.texCoords,0);
}

technique11 downscale_depth
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepth()));
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