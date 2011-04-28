
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
int octaves;
float persistence;

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

v2f MainVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 MainPS(v2f IN) : COLOR
{
	float3 c=float3(0,0,0);
	float scale=.5f;
	int i;
	float2 sc;
	float r;
	float az;
	for(i=0;i<octaves;i++)
	{
		float4 lookup=tex2D(noise_texture,IN.texcoord);
		r=1.0-pow(lookup.x,4.0);
		az=lookup.y*6.283;
		sincos(az,sc.x,sc.y);
		float sinel=lookup.z*2.0-1.0;
		float cosel=sqrt(1.0-sinel*sinel);
		sc*=cosel;
		float3 random_dir=float3(sc.x,sc.y,sinel);
		float3 newc=scale*r*random_dir;
		c+=newc;
		scale*=persistence;
		IN.texcoord*=2.0;
	}
	c+=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
    return float4(c.rrr,c.r);
}

technique simul_rendernoise
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_2_0 MainVS();
		PixelShader = compile ps_3_0 MainPS();
    }
}