#include "CppHlsl.hlsl"

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

v2f DebugVS(idOnly IN)
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
/*
v2f DebugVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = IN.colour;
    return OUT;
}
*/

float4 DebugPS(v2f IN) : SV_TARGET
{
    return IN.colour;
}

DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = LESS_EQUAL;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

technique11 simul_debug
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,DebugVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}
