#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/mixed_resolution_constants.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/mixed_resolution.sl"
Texture2DMS<float4> sourceMSDepthTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> sourceTextureMS SIMUL_TEXTURE_REGISTER(0);
Texture2D<float4> sourceDepthTexture SIMUL_TEXTURE_REGISTER(0);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

[numthreads(8,8,1)]
void CS_Resolve(uint3 pos : SV_DispatchThreadID )
{
	Resolve(sourceTextureMS,target2DTexture,pos.xy);
}

[numthreads(8,8,1)]
void CS_DownscaleDepthFarNear(uint3 pos : SV_DispatchThreadID )
{
	DownscaleDepthFarNear(sourceMSDepthTexture,target2DTexture,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_ResolveDepth(posTexVertexOutput IN):SV_Target
{
	uint2 source_dims;
	uint numberOfSamples;
	sourceMSDepthTexture.GetDimensions(source_dims.x,source_dims.y,numberOfSamples);
	uint2 hires_pos		=uint2(vec2(source_dims)*IN.texCoords.xy);
	return sourceMSDepthTexture.Load(hires_pos,0).x;
}

technique11 resolve
{
    pass p0
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Resolve()));
    }
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