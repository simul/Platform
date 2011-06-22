float4x4 worldViewProj	: WorldViewProjection;
float offset;
float intensity;
float4 lightColour;
float2 offset1;
float2 offset2;
float2 offset3;
sampler2D rainTexture:TEXUNIT0= sampler_state 
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
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

float4 PS_Main(vertexOutput IN): color
{
	float2 offs=float2(0,offset);
	float2 offs1=offs+offset1;
	float2 offs2=offs+offset2;
	float2 offs3=offs+offset3;
	float4 fade=IN.texCoords.z;
	float4 colour1=fade*tex2D(rainTexture,IN.texCoords.xy+offs1);
	IN.texCoords.xy*=2;
	float4 colour2=0.7f*fade*tex2D(rainTexture,IN.texCoords.xy+offs2);
	IN.texCoords.xy*=2;
	float4 colour3=0.5f*fade*tex2D(rainTexture,IN.texCoords.xy+offs3);
    float4 colour=colour1+colour2+colour3;
    colour-=(1.f-intensity);
	colour.a=intensity*colour.r;
    colour=saturate(colour);
    colour.rgb*=lightColour.rgb;
    return colour;
}

technique simul_rain
{
    pass p0 
    {
		zenable = false;
		ZWriteEnable = false;
        AlphaBlendEnable = true;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
		SrcBlend = One;
		DestBlend = InvSrcAlpha;
		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();
    }
}

