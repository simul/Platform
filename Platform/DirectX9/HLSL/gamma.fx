#include "dx9.hlsl"
texture hdrTexture;
sampler2D hdr_texture= sampler_state 
{
    Texture = <hdrTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
	SRGBTexture = 0;
};

float exposure;
float gamma=1.f/2.2f;

struct a2v
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
};

struct v2f
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
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

float4 GammaPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    c.rgb*=exposure;
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
	c.rgb=pow(c.rgb,gamma);
    return float4(c.rgb,0.f);
}

float4 BlendPS(v2f IN) : COLOR
{
	float4 c=tex2D(hdr_texture,IN.texcoord);
    return c;
}

float4 PS_CloudBlend(vertexOutputPosTexc IN) : COLOR
{
	vec4 c=tex2D(hdr_texture,vec2(IN.texCoords.x,1.0-IN.texCoords.y));
	//if(c.a>=1.f)
	//	discard;
    vec4 result=vec4(c.rgb*exposure,c.a);

	return result;
}

technique cloud_blend
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
		VertexShader = compile vs_3_0 VS_FullScreen();
		PixelShader  = compile ps_3_0 PS_CloudBlend();
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

technique simul_sky_over_stars
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