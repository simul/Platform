
texture hdrTexture;
sampler2D hdr_texture= sampler_state 
{
    Texture = <hdrTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

texture bloomTexture;
sampler2D bloom_texture= sampler_state 
{
    Texture = <bloomTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
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
    float4 position  : POSITION;
    float4 texcoord  : TEXCOORD0;
};

v2f TonemapVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texcoord = IN.texcoord;
    return OUT;
}

v2f TonemapZWriteVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.position.z=0.1f*OUT.position.w;
	OUT.texcoord = IN.texcoord;
    return OUT;
}
float brightpassThreshold;
float2 brightpassOffsets[4];
#ifndef BLUR_SIZE
	#define BLUR_SIZE 65
#endif
float2 bloomOffsets[BLUR_SIZE];
float bloomWeights[BLUR_SIZE];

float4 BrightPassPS(v2f IN) : COLOR
{
    float4 average={0.0f,0.0f,0.0f,0.0f};
	for(int i=0;i<4;i++)
		average+=tex2D(hdr_texture,IN.texcoord+brightpassOffsets[i]);
	average*=0.25f;
	float luminance=0.27f*average.r+0.67f*average.g+0.06*average.b;
	if(luminance<brightpassThreshold)
		average=float4(0.0f,0.0f,0.0f,1.0f);
	average*=exposure;
	average=min(average,float4(1.f,1.f,1.f,1.f));
	return average;
}

float4 BlurPS(v2f IN) : COLOR
{
    float4 colour={0.0f,0.0f,0.0f,0.0f};
    for(int i=0;i<BLUR_SIZE;i++)
	{
	   colour+=tex2D(hdr_texture,IN.texcoord+bloomOffsets[i])*bloomWeights[i];
	}
    return float4(colour.rgb,1.0f);
}

float4 GammaPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    c.rgb*=exposure;
    // gamma correction - could use texture lookups for this
    c.rgb=pow(c.rgb, gamma);
    return float4(c.rgb,0.f);
}

float4 DirectPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    return c;
}

float4 TonemapPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    c.rgb*=exposure;
	//c+=tex2D(bloom_texture,IN.texcoord);
    c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,0.f);
}

float4 BlendPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    return c;
}

float4 CloudBlendPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
	if(c.a>=1.f)
		discard;
    return c;
}

technique simul_cloud_blend
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = SrcAlpha;
		AlphaTestEnable=false;
		COLORWRITEENABLE=15;
		VertexShader = compile vs_3_0 TonemapVS();
		PixelShader = compile ps_3_0 CloudBlendPS();
    }
}

technique simul_brightpass
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		VertexShader = compile vs_3_0 TonemapVS();
		PixelShader = compile ps_3_0 BrightPassPS();
    }
}

technique simul_blur
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		VertexShader = compile vs_3_0 TonemapVS();
		PixelShader = compile ps_3_0 BlurPS();
    }
}

technique simul_gamma_zwrite
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = true;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		VertexShader = compile vs_3_0 TonemapVS();
		PixelShader = compile ps_3_0 GammaPS();
    }
}

technique simul_gamma
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = false;
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
		AlphaTestEnable=false;
		VertexShader = compile vs_2_0 TonemapVS();
		PixelShader = compile ps_2_0 GammaPS();
    }
}

technique simul_direct
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = false;
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = SrcAlpha;
		AlphaTestEnable=false;
		VertexShader = compile vs_2_0 TonemapVS();
		PixelShader = compile ps_2_0 DirectPS();
    }
}

technique simul_tonemap
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		COLORWRITEENABLE=15;
		VertexShader = compile vs_3_0 TonemapVS();
		PixelShader = compile ps_3_0 TonemapPS();
    }
}


technique simul_tonemap_zwrite
{
    pass p0
    {
		cullmode = none;
		ZEnable = true;
		ZWriteEnable = true;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		COLORWRITEENABLE=15;
		VertexShader = compile vs_3_0 TonemapZWriteVS();
		PixelShader = compile ps_3_0 TonemapPS();
    }
}