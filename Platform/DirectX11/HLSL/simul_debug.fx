#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/depth.sl"

sampler2D imageTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> imageTextureMS SIMUL_TEXTURE_REGISTER(1);
TextureCube cubeTexture SIMUL_TEXTURE_REGISTER(2);

uniform_buffer DebugConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform int latitudes,longitudes;
	uniform float radius;
	uniform float multiplier;
	uniform vec4 colour;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec2 tanHalfFov;
};

cbuffer cbPerObject : register(b11)
{
	float4 rect;
};

struct a2v
{
    float3 position	: POSITION;
    float4 colour	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float4 colour		: TEXCOORD0;
};

posTexVertexOutput Debug2DVS(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}

v2f DebugVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition	=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour		=IN.colour;
    return OUT;
}

v2f Debug2dVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition	=float4(rect.xy+rect.zw*IN.position.xy,0.0,1.0);
	OUT.colour		=IN.colour;
    return OUT;
}

v2f CircleVS(idOnly IN)
{
	v2f OUT;
	float angle		=2.0*3.1415926536*float(IN.vertex_id)/31.0;
	vec4 pos		=vec4(100.0*vec3(radius*vec2(cos(angle),sin(angle)),1.0),1.0);
    OUT.hPosition	=mul(worldViewProj,float4(pos.xyz,1.0));
	OUT.colour		=colour;
    return OUT;
}

v2f FilledCircleVS(idOnly IN)
{
	v2f OUT;
	int i=int(IN.vertex_id/2);
	int j=int(IN.vertex_id%2);
	float angle		=2.0*3.1415926536*float(IN.vertex_id)/31.0;
	vec4 pos		=vec4(100.0*vec3(radius*j*vec2(cos(angle),sin(angle)),1.0),1.0);
    OUT.hPosition	=mul(worldViewProj,float4(pos.xyz,1.0));
	OUT.colour		=colour;
    return OUT;
}

float4 DebugPS(v2f IN) : SV_TARGET
{
    return IN.colour;
}

vec4 TexturedPS(posTexVertexOutput IN) : SV_TARGET
{
	vec4 res=multiplier*texture_nearest_lod(imageTexture,IN.texCoords.xy,0);
	return res;
}

vec4 TexturedMSPS(posTexVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	uint numSamples;
	imageTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	uint2 pos	=uint2(IN.texCoords.xy*vec2(dims.xy));
	vec4 res	=multiplier*imageTextureMS.Load(pos,0);
	return res;
}
float depthToFadeDistance(vec4 depth,vec2 xy,vec3 depthToLinFadeDistParams,vec2 tanHalf)
{
	vec4 linearFadeDistanceZ = depthToLinFadeDistParams.xxxx / (depth*depthToLinFadeDistParams.yyyy + depthToLinFadeDistParams.zzzz);
	float Tx=xy.x*tanHalf.x;
	float Ty=xy.y*tanHalf.y;
	vec4 fadeDist = linearFadeDistanceZ * sqrt(1.0+Tx*Tx+Ty*Ty);
	return fadeDist;
}

vec4 ShowDepthPS(posTexVertexOutput IN) : SV_TARGET
{
	vec4 depth		=texture_clamp(imageTexture,IN.texCoords);
	vec4 dist		=depthToFadeDistance(depth,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
    return vec4(dist.xy,depth.z,1.0);
}
struct vec3input
{
    float3 position	: POSITION;
};

v2f Vec3InputSignatureVS(vec3input IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = float4(1.0,1.0,1.0,1.0);
    return OUT;
}

struct v2f_cubemap
{
    float4 hPosition	: SV_POSITION;
    float3 wDirection	: TEXCOORD0;
};


v2f_cubemap VS_DrawCubemap(vec3input IN) 
{
    v2f_cubemap OUT;
    OUT.hPosition	=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

v2f_cubemap VS_DrawCubemapSphere(idOnly IN) 
{
    v2f_cubemap OUT;
	// we have (latitudes+1)*(longitudes+1)*2 id's
	uint vertex_id		=IN.vertex_id;
	uint latitude_strip	=vertex_id/(longitudes+1)/2;
	vertex_id			-=latitude_strip*(longitudes+1)*2;
	uint longitude		=(vertex_id)/2;
	vertex_id			-=longitude*2;
	float azimuth		=2.0*3.1415926536*float(longitude)/float(longitudes);
	float elevation		=(float(latitude_strip+vertex_id)/float(latitudes)-0.5)*3.1415926536;
	vec3 pos			=radius*vec3(sin(azimuth)*cos(elevation),cos(azimuth)*cos(elevation),sin(elevation));
    OUT.hPosition		=mul(worldViewProj,float4(pos.xyz,1.0));
    OUT.wDirection		=normalize(pos.xyz);
    return OUT;
}

SamplerState cubeSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Mirror;
	AddressV = Mirror;
	AddressW = Mirror;
};

float4 PS_DrawCubemap(v2f_cubemap IN): SV_TARGET
{
	float3 view		=(IN.wDirection.xyz);
	float4 result	=cubeTexture.Sample(cubeSamplerState,view);
	return float4(result.rgb,1.f);
}

technique11 simul_debug
{
    pass p0
    {
		SetRasterizerState( wireframeRasterizer );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,DebugVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}
technique11 lines_2d
{
    pass p0
    {
		SetRasterizerState( wireframeRasterizer );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Debug2dVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}
fxgroup circle
{
	technique11 outline
	{
		pass p0
		{
			SetRasterizerState( wireframeRasterizer );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,CircleVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
	}
	technique11 filled
	{
		pass p0
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,FilledCircleVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
	}
}

technique11 textured
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Debug2DVS()));
		SetPixelShader(CompileShader(ps_4_0,TexturedPS()));
    }
}

technique11 texturedMS
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,Debug2DVS()));
		SetPixelShader(CompileShader(ps_5_0,TexturedMSPS()));
    }
}

technique11 vec3_input_signature
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Vec3InputSignatureVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}

technique11 draw_cubemap
{
    pass p0 
    {		
		SetRasterizerState( RenderBackfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemap()));
		SetPixelShader(CompileShader(ps_4_0,PS_DrawCubemap()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}
technique11 draw_cubemap_sphere
{
    pass p0 
    {		
		SetRasterizerState( RenderBackfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemapSphere()));
		SetPixelShader(CompileShader(ps_4_0,PS_DrawCubemap()));
		SetDepthStencilState( EnableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}

technique11 show_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Debug2DVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}