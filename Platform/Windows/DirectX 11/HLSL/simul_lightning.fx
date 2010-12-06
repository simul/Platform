float4x4 worldViewProj	: WorldViewProjection;

texture lightningTexture;
sampler2D lightning_texture= sampler_state 
{
    Texture = <lightningTexture>;
	Filter = MIN_MAG_MIP_LINEAR;
};

struct vertexInput
{
    float3 position		: POSITION;
    float2 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: POSITION;
    float2 texCoords	: TEXCOORD0;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
    return OUT;
}

half4 PS_Main(vertexOutput IN): color
{
	half4 colour=tex2D(lightning_texture,IN.texCoords.xy);
    return colour;
}

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

BlendState DoBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = One;
	DestBlend = Inv_Src_Alpha;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

technique11 simul_lightning
{
    pass p0 
    {
		SetDepthStencilState(DisableDepth,0);
        SetRasterizerState(RenderNoCull);
		SetBlendState(DoBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Main()));
		SetPixelShader(CompileShader(ps_5_0,PS_Main()));
    }
}

