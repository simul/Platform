#include "CppHlsl.hlsl"
#include "states.hlsl"
Texture2D<float4> source2DTexture;
RWTexture2D<float> target2DTexture;

[numthreads(8,8,1)]
void CS_DownscaleDepth(uint3 pos : SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	uint2 hires_pos[]={2*pos,2*pos+uint2(1,0),2*pos+uint2(0,1),2*pos+uint2(1,1)};
	vec4 values[4];
	float farthest_depth=1.0;
	for(int i=0;i<4;i++)
	{
		vec4 lookup=source2DTexture[hires_pos[i]];
		if(lookup.x<farthest_depth)
			farthest_depth=lookup.x;
	}
	targetTexture[idx]	=farthest_depth;
}

technique11 downscale_depth
{
    pass p0
    {
		SetPixelShader(CompileShader(ps_4_0,CS_DownscaleDepth()));
    }
}