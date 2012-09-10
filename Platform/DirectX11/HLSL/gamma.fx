Texture2D hdrTexture;
float4x4 worldViewProj	: WorldViewProjection;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

float exposure=1.f;
float gamma=1.f/2.2f;

struct a2v
{
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float2 texcoord		: TEXCOORD0;
};

v2f TonemapVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition.xy=IN.position.xy;
	OUT.hPosition.z=0.05*IN.position.z;
	OUT.hPosition.w=0.5*IN.position.z+IN.position.w;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 TonemapPS(v2f IN) : SV_TARGET
{
	float4 c=hdrTexture.Sample(samplerState,IN.texcoord);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,1.f);
}

float4 DirectPS(v2f IN) : SV_TARGET
{
	float4 c=hdrTexture.Sample(samplerState,IN.texcoord);
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

technique11 simul_direct
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,TonemapVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
    }
}


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
