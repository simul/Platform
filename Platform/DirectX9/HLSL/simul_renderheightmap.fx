
texture noiseTexture;
sampler2D noise_texture= sampler_state 
{
    Texture = <noiseTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture heightTexture;
sampler2D height_texture=sampler_state
{
    Texture = <heightTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

texture remapHeightsTexture;
sampler1D remap_heights_texture=sampler_state
{
    Texture = <remapHeightsTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

int octaves;
float persistence;
float2 heightRange;
float texcoordScale;
float worldSize;

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

v2f MainVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 FractalHeightPS(v2f IN) : COLOR
{
	float4 c=float4(0,0,0,0);
	float scale=.5f;
	int i;
	float2 texcoord=IN.texcoord*texcoordScale;
	for(i=0;i<octaves;i++)
	{
		float4 lookup=tex2D(noise_texture,texcoord);
		float newc=scale*2.0*(lookup.r-0.5);
		c+=float4(newc,newc,newc,newc);
		scale*=persistence;
		texcoord*=2.f;
		//total+=mult;
	}
	c+=float4(0.5f,0.5f,0.5f,0.5f);
    return c;
}

float4 RidgePS(v2f IN) : COLOR
{
	float h=tex2D(height_texture,IN.texcoord).r;
	if(h>1.0)
		h=1.0;
	h=tex1D(remap_heights_texture,h).r;
	return float4(h,h,h,h);
}

float4 HeightToNormalsPS(v2f IN) : COLOR
{
	float h=tex2D(height_texture,IN.texcoord).r;
	float dx=ddx(h);
	float dy=ddy(h);
	float3 n=float3(-dx,-dy,1.0/2.0);
	n=normalize(n);
	h*=heightRange.y;
	h+=heightRange.x;
	return float4(h,n);
}

technique fractal_height
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 FractalHeightPS();
    }
}
// Ridge filter:
technique ridge
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 RidgePS();
    }
}

technique height_to_normals
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 HeightToNormalsPS();
    }
}