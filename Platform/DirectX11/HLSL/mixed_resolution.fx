#include "CppHlsl.hlsl"
#include "states.hlsl"
Texture2D<float4> source2DTexture SIMUL_TEXTURE_REGISTER(0);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

[numthreads(8,8,1)]
void CS_DownscaleDepth(uint3 pos : SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	uint2 src_dims;
	source2DTexture.GetDimensions(src_dims.x,src_dims.y);

	uint2 scale=uint2(src_dims.x/dims.x,src_dims.y/dims.y);

	uint2 pos2			=pos.xy*scale;
	vec4 values[4];
	float farthest_depth=1.0;
	for(int i=0;i<scale.x;i++)
	{
		for(int j=0;j<scale.y;j++)
		{
			uint2 hires_pos	=pos2+uint2(i,j);
			vec4 lookup=source2DTexture[hires_pos];
			if(lookup.x<farthest_depth)
				farthest_depth=lookup.x;
		}
	}
	target2DTexture[pos.xy]	=farthest_depth;
}
[numthreads(1,1,1)]
void CS_InterpLightTable(uint3 pos	: SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	target2DTexture[pos.xy]	=0.5;
}

technique11 downscale_depth
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepth()));
    }
}