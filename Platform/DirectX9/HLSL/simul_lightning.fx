float4x4 worldViewProj	: WorldViewProjection;

sampler2D lightningTexture:TEXUNIT0= sampler_state 
{
	AddressU = Clamp;
	AddressV = Wrap;
};
float4 lightningColour;

struct vertexInput
{
    float3 position		: POSITION;
    float2 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    float4 hPosition	: POSITION;
    float2 texCoords	: TEXCOORD0;
	float size			: PSIZE;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul(worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
	OUT.size=IN.texCoords.y;
    return OUT;
}

float4 PS_Lines(vertexOutput IN): color
{
	float4 colour=lightningColour*float4(1.0,1.0,1.0,IN.texCoords.y);
    return colour;
}

float4 PS_Quads(vertexOutput IN): color
{
	float4 colour=IN.texCoords.y*tex2D(lightningTexture,float2(IN.texCoords.x,0.5));//lightningColour*
	colour.a=1;
    return colour;
}


technique simul_lightning_lines
{
    pass p0 
    {
		zenable = true;
		ZWriteEnable = false;
        AlphaBlendEnable = true;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
		SrcBlend = SrcAlpha;
		DestBlend = One;
		
		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Lines();
    }
}


technique simul_lightning_quads
{
    pass p0 
    {
		zenable = true;
		ZWriteEnable = false;
        AlphaBlendEnable = true;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
		SrcBlend = SrcAlpha;
		DestBlend = One;
		
		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Quads();
    }
}

