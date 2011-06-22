float4x4 worldViewProj	: WorldViewProjection;
float offset;
float intensity;
float4 lightColour;
texture2D rainTexture;
SamplerState rainSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct vertexInput
{
    float3 position		: POSITION;
    float3 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: POSITION;
    float3 texCoords	: TEXCOORD0;		/// z is intensity!
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_Main(vertexOutput IN): SV_TARGET
{
	float2 offs=float2(0,offset);
	float4 fade=IN.texCoords.z;
	float4 colour1=fade*rainTexture.Sample(rainSampler,IN.texCoords.xy+offs);
	IN.texCoords.y*=4;
	float4 colour2=0.7f*fade*rainTexture.Sample(rainSampler,IN.texCoords.xy+2*offs);
	IN.texCoords.y*=4;
	float4 colour3=0.5f*fade*rainTexture.Sample(rainSampler,IN.texCoords.xy+4*offs);
    float4 colour=colour1+colour2+colour3;
    colour-=(1.f-intensity);
	colour.a=intensity;
    colour=saturate(colour);
    colour*=lightColour;
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
	DestBlend = One;
};
RasterizerState RenderNoCull { CullMode = none; };

technique11 simul_rain
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
		SetPixelShader(CompileShader(ps_4_0,PS_Main()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

