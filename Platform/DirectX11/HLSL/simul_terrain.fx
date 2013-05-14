cbuffer cbPerObject : register(b0)
{
	matrix worldViewProj : packoffset(c0);
};

Texture2D mainTexture;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

float maxFadeDistanceMetres ;

float morphFactor;
float4 eyePosition;
float3 lightDir;

float3 cloudScales;	
float3 cloudOffset;
float3 lightColour;
float3 ambientColour;
float cloudInterp;

struct vertexInput
{
    float3 position			: POSITION;
    float3 normal			: TEXCOORD0;
    float2 texCoordDiffuse	: TEXCOORD1;
    float offset			: TEXCOORD2;
};

struct vertexOutput
{
    float4 hPosition		: SV_POSITION;
    float4 normal			: TEXCOORD0;
    float2 texCoordDiffuse	: TEXCOORD1;
    float4 wPosition		: TEXCOORD2;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz,1.f));
    OUT.wPosition = float4(IN.position.xyz,1.f);
    OUT.texCoordDiffuse=IN.texCoordDiffuse;
    OUT.normal.xyz=IN.normal;
    OUT.normal.a=0.5;
    return OUT;
}

float4 PS_Main( vertexOutput IN) : SV_TARGET
{
	float depth=length(IN.wPosition-eyePosition)/maxFadeDistanceMetres;
	float3 final=mainTexture.Sample(samplerState,IN.texCoordDiffuse.xy).rgb;
    return float4(final,depth);
}

DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
}; 

BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};

RasterizerState RenderNoCull { CullMode = none; };//back front

technique11 simul_terrain
{
    pass base 
    {
		SetRasterizerState(RenderNoCull);
		SetDepthStencilState(EnableDepth,0);
		SetBlendState(DontBlend,float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
    }
}
