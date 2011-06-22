cbuffer cbPerObject : register(b0)
{
	matrix worldViewProj : packoffset(c0);
};

Texture2D lightningTexture;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
};

struct vertexInput
{
    float3 position		: POSITION;
    float2 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
    float2 texCoords	: TEXCOORD0;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_Main(vertexOutput IN): SV_TARGET
{
	float4 colour=lightningTexture.Sample(samplerState,IN.texCoords.xy);
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

