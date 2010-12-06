Texture2D hdrTexture;
	matrix worldViewProj;
SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float exposure;
float gamma=1.f/2.2f;

struct a2v
{
    float4 position  : POSITION;
    float4 texcoord  : TEXCOORD0;
};

struct v2f
{
    float4 position  : SV_POSITION;
    float4 texcoord  : TEXCOORD0;
};

v2f TonemapVS(a2v IN)
{
	v2f OUT;
    OUT.position=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 TonemapPS(v2f IN) : SV_TARGET
{
	float4 c=hdrTexture.Sample(samplerState,IN.texcoord.xy);
    c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,1.f);
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

technique11 simul_tonemap
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,TonemapPS()));
    }
}
