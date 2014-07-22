#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/mixed_resolution_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/colour_packing.sl"
#include "../../CrossPlatform/SL/mixed_resolution.sl"
Texture2DMS<float4> sourceMSDepthTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> sourceTextureMS SIMUL_TEXTURE_REGISTER(0);
Texture2D<float4> sourceDepthTexture SIMUL_TEXTURE_REGISTER(1);
RWTexture2D<float4> target2DTexture SIMUL_RWTEXTURE_REGISTER(1);

#define BLOCK_SIZE 8

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
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	//if(pos.y>target_dims.y/2)
	//	return vec4(0,0,0,0);
	vec4 res	=DownscaleDepthFarNear(sourceDepthTexture,source_dims,pos,scale,depthToLinFadeDistParams);
	return res;
}

vec4 PS_DownscaleDepthFarNear_MSAA1(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,1,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA2(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,2,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA4(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	vec4 r= DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,4,pos,scale,depthToLinFadeDistParams);
	return r;
}

vec4 PS_DownscaleDepthFarNear_MSAA8(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,8,pos,scale,depthToLinFadeDistParams);
}

vec4 PS_DownscaleDepthFarNear_MSAA16(posTexVertexOutput IN):SV_Target
{
	//vec2 texCoords=IN.texCoords.xy-vec2(0.5,0.5)/vec2(target_dims.xy);
	int2 pos	=int2(IN.texCoords.xy*target_dims.xy);
	return DownscaleDepthFarNear_MSAA(sourceMSDepthTexture,source_dims,16,pos,scale,depthToLinFadeDistParams);
}


	
#if 1
[numthreads(1,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear(uint3 pos : SV_DispatchThreadID )
{
	vec4 res	=DownscaleDepthFarNear(sourceDepthTexture,source_dims,pos.xy,scale,depthToLinFadeDistParams);
	//res.xyzw=vec4(1,1,1,1);
	target2DTexture[pos.xy]=res;
}
#else
// In this version we do a whole row at once.
[numthreads(BLOCK_SIZE,BLOCK_SIZE,1)]
void CS_DownscaleDepthFarNear(uint3 pos : SV_DispatchThreadID )
{
	// pos is the texel position in the target.
	int2 pos2=int2(pos.x*64-1,pos.y*2-1);
	vec4 source[66];
	if(pos2.y>=0)
	for(int i=0;i<66;i++)
	{
		source[i].x=sourceDepthTexture[int2(pos2.x+i,pos2.y)].x;
		source[i].y=sourceDepthTexture[int2(pos2.x+i,pos2.y+1)].x;
		source[i].z=sourceDepthTexture[int2(pos2.x+i,pos2.y+2)].x;
		source[i].w=sourceDepthTexture[int2(pos2.x+i,pos2.y+3)].x;
	}
	else
	for(int i=0;i<66;i++)
	{
		source[i].y=sourceDepthTexture[int2(pos2.x+i,pos2.y+1)].x;
		source[i].z=sourceDepthTexture[int2(pos2.x+i,pos2.y+2)].x;
		source[i].w=sourceDepthTexture[int2(pos2.x+i,pos2.y+3)].x;
		source[i].x=source[i].y;
	}
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.
	for(int k=0;k<32;k++)
	{
		#if REVERSE_DEPTH==1
			float nearest_depth			=0.0;
			float farthest_depth		=1.0;
		#else
			float nearest_depth			=1.0;
			float farthest_depth		=0.0;
		#endif
		int kpos=k*2;
		for(int i=0;i<4;i++)
		{
			int ipos=kpos+i;
			vec4 u					=source[ipos];
			for(int j=0;j<4;j++)
			{
				float d				=u[j];
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
		float edge		=0.0;
		if(nearest_depth!=farthest_depth)
		{
			float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
			float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
			edge		=f-n;
			edge		=step(0.002,edge);
		}
		vec4 res		=vec4(farthest_depth,nearest_depth,edge,0);
		int2 target_pos	=int2(pos.x*32+k,pos.y);
		target2DTexture[target_pos.xy]=res;
	}
}
#endif

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
	target2DTexture[pos.xy]=DownscaleDepthFarNear_MSAA4(sourceMSDepthTexture,source_dims,pos.xy,scale,depthToLinFadeDistParams);
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