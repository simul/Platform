#include "CppHlsl.hlsl"
#include "states.hlsl"
TextureCube cubeTexture;

float4x4 worldViewProj	: WorldViewProjection;

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

v2f Debug2DVS(idOnly IN)
{
	v2f OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=float4(pos,1.0,1.0);
	float2 texc2 = .5*(float2(1.0,1.0)+vec2(pos.x,pos.y));
	OUT.colour	= float4(texc2, 0,0);
	return OUT;
}

v2f DebugVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = IN.colour;
    return OUT;
}

float4 DebugPS(v2f IN) : SV_TARGET
{
    return IN.colour;
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
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,DebugVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}

technique11 vec3_input_signature
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}