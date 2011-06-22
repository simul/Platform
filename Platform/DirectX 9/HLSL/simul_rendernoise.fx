
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
	float4 c=float4(0,0,0,0);
	float scale=.5f;
	int i;
	//float total=0.f;
	for(i=0;i<octaves;i++)
	{
		float4 lookup=tex2D(noise_texture,IN.texcoord);
		float r=1.f-pow(lookup.x,4.f);
		float az=lookup.y*2*3.1415926536f;
		float el=asin(lookup.z*2.f-1.f);
		float4 newc=scale*float4(r*sin(az)*cos(el),r*cos(az)*cos(el),r*sin(el),0);
		c+=newc;
		scale*=persistence;
		IN.texcoord*=2.f;
		//total+=mult;
	}
	c+=float4(0.5f,0.5f,0.5f,0.5f);
	//c/=total;
	//c.r=0.3f;
    return c;
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