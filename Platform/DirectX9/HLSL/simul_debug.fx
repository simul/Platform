float4x4 worldViewProj : WorldViewProjection;

struct colourVertexInput
{
    float3 position			: POSITION;
    float4 colour			: COLOR0;
};

struct colourVertexOutput
{
    float4 hPosition		: POSITION;
    float4 colour			: COLOR0;
};

colourVertexOutput VS_Plain_Colour(colourVertexInput IN) 
{
    colourVertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.colour=IN.colour;
    return OUT;
}

float4 PS_Plain_Colour(colourVertexOutput IN): color
{
	float4 re=IN.colour;
	re.a=1.0;
	return re;
}


technique simul_plain_colour
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Plain_Colour();
		PixelShader  = compile ps_2_b PS_Plain_Colour();
       
        CullMode = None;
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
		SrcBlend = SrcAlpha;
		DestBlend = One;
#ifndef XBOX
		lighting = false;
#endif
    }
}