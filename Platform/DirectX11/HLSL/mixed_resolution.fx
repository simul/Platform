#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/mixed_resolution_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/mixed_resolution.sl"
Texture2DMS<float4> sourceMSDepthTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> sourceTextureMS SIMUL_TEXTURE_REGISTER(0);
Texture2D<float4> sourceDepthTexture SIMUL_TEXTURE_REGISTER(1);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

#define BLOCK_SIZE 12

vec4 PS_MakeDepthFarNear(posTexVertexOutput IN):SV_Target
{
	uint2 pos=uint2(IN.texCoords.xy*source_dims.xy);
	vec4 res=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,1,pos,depthToLinFadeDistParams);
	return res;
}

vec4 PS_MakeDepthFarNear_MSAA1(posTexVertexOutput IN):SV_Target
{
	return MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,1,uint2(IN.texCoords.xy*source_dims.xy),depthToLinFadeDistParams);
}

vec4 PS_MakeDepthFarNear_MSAA2(posTexVertexOutput IN):SV_Target
{
	return MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,2,uint2(IN.texCoords.xy*source_dims.xy),depthToLinFadeDistParams);
}

vec4 PS_MakeDepthFarNear_MSAA4(posTexVertexOutput IN):SV_Target
{
	uint2 pos=uint2(IN.texCoords.xy*source_dims.xy);
	vec4 r= MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,4,pos,depthToLinFadeDistParams);
	return r;
}

vec4 PS_MakeDepthFarNear_MSAA8(posTexVertexOutput IN):SV_Target
{
	return MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,8,uint2(IN.texCoords.xy*source_dims.xy),depthToLinFadeDistParams);
}

vec4 PS_MakeDepthFarNear_MSAA16(posTexVertexOutput IN):SV_Target
{
	return MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,16,uint2(IN.texCoords.xy*source_dims.xy),depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]	=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,1,pos.xy,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear_MSAA1(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,1,pos.xy,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear_MSAA2(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,2,pos.xy,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear_MSAA4(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,4,pos.xy,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear_MSAA8(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,8,pos.xy,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_MakeDepthFarNear_MSAA16(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=MakeDepthFarNear(sourceDepthTexture,sourceMSDepthTexture,16,pos.xy,depthToLinFadeDistParams);
}







[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_Resolve(uint3 pos : SV_DispatchThreadID )
{
	Resolve(sourceTextureMS,target2DTexture,pos.xy);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNearFromHiRes(uint3 pos : SV_DispatchThreadID )
{
	uint2 dims;
	target2DTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	target2DTexture[pos.xy]=DownscaleFarNearEdge(sourceDepthTexture,source_dims,pos.xy,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear(posTexVertexOutput IN):SV_Target
{
	// RVK: Note - here we will receive a texture coordinate, starting at HALF A PIXEL.
	// But we don't want that, because we want values starting at zero.
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	//if(texCoords.x<0.49f/target_dims.x)
	//	return vec4(0,0,0,0);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	//if(pos.y>target_dims.y/2)
	//	return vec4(0,0,0,0);
	vec4 res	=DownscaleDepthFarNear(sourceDepthTexture,source_dims,pos,scale,depthToLinFadeDistParams);
	return res;
}

vec4 PS_DownscaleDepthFarNear_MSAA1(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,1,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA2(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,2,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA4(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	vec4 r= DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,4,pos,scale,depthToLinFadeDistParams);
	return r;
}

vec4 PS_DownscaleDepthFarNear_MSAA8(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,8,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA16(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	uint2 pos	=uint2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,16,pos,scale,depthToLinFadeDistParams);
}


	

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear(uint3 pos : SV_DispatchThreadID )
{
	vec4 res	=DownscaleDepthFarNear(sourceDepthTexture,source_dims,pos.xy,scale,depthToLinFadeDistParams);
	//res.xyzw=vec4(1,1,1,1);
	target2DTexture[pos.xy]=res;
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear_MSAA1(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,1,pos.xy,scale,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear_MSAA2(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,2,pos.xy,scale,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear_MSAA4(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,4,pos.xy,scale,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear_MSAA8(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,8,pos.xy,scale,depthToLinFadeDistParams);
}

[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear_MSAA16(uint3 pos : SV_DispatchThreadID )
{
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,16,pos.xy,scale,depthToLinFadeDistParams);
}



[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_SpreadEdge(uint3 pos : SV_DispatchThreadID )
{
	SpreadEdge(sourceDepthTexture,target2DTexture,pos.xy);
}
vec4 PS_ResolveDepth(posTexVertexOutput IN):SV_Target
{
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

technique11 downscale_depth_far_near_from_hires
{
    pass main
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNearFromHiRes()));
    }
}

technique11 spread_edge
{
    pass main
    {
		SetComputeShader(CompileShader(cs_5_0,CS_SpreadEdge()));
    }
}

technique11 make_depth_far_near
{
    pass main
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear()));
    }
    pass msaa1
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear_MSAA1()));
    }
    pass msaa2
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear_MSAA2()));
    }
    pass msaa4
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear_MSAA4()));
    }
    pass msaa8
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear_MSAA8()));
    }
    pass msaa16
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_MakeDepthFarNear_MSAA16()));
    }
}

technique11 downscale_depth_far_near
{
    pass main
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear()));
    }
    pass msaa1
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear_MSAA1()));
    }
    pass msaa2
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear_MSAA2()));
    }
    pass msaa4
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear_MSAA4()));
    }
    pass msaa8
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear_MSAA8()));
    }
    pass msaa16
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0),0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_DownscaleDepthFarNear_MSAA16()));
    }
}
technique11 cs_make_depth_far_near
{
    pass main
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear()));
    }
    pass msaa1
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear_MSAA1()));
    }
    pass msaa2
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear_MSAA2()));
    }
    pass msaa4
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear_MSAA4()));
    }
    pass msaa8
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear_MSAA8()));
    }
    pass msaa16
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeDepthFarNear_MSAA16()));
    }
}
technique11 cs_downscale_depth_far_near
{
    pass main
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear()));
    }
    pass msaa1
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear_MSAA1()));
    }
    pass msaa2
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear_MSAA2()));
    }
    pass msaa4
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear_MSAA4()));
    }
    pass msaa8
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear_MSAA8()));
    }
    pass msaa16
    {
		SetComputeShader(CompileShader(cs_5_0,CS_DownscaleDepthFarNear_MSAA16()));
    }
}

technique11 resolve_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_5_0,VS_SimpleFullscreen()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0,PS_ResolveDepth()));
    }
}