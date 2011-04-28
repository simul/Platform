float4x4 worldViewProj	: WorldViewProjection;
float4x4 world			: World;

texture g_mainTexture;
sampler mainTexture = sampler_state
{
    Texture = <g_mainTexture>;
	AddressU = Wrap;
	AddressV = Wrap;
};
texture grassTexture;
sampler grass_texture = sampler_state
{
    Texture = <grassTexture>;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture cloudTexture1;
sampler3D cloud_texture1= sampler_state 
{
    Texture = <cloudTexture1>;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};
texture cloudTexture2;
sampler3D cloud_texture2= sampler_state 
{
    Texture = <cloudTexture2>;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
};

float morphFactor;
float4 eyePosition;
float4 lightDir;

float3 cloudScales;	
float3 cloudOffset;
float3 lightColour;
float3 ambientColour;
float cloudInterp;

struct vertexInput
{
    float3 position			: POSITION;
    float4 normal			: TEXCOORD0;
    float2 texCoordDiffuse	: TEXCOORD1;
    float offset			: TEXCOORD2;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
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
    OUT.normal=IN.normal;
    return OUT;
}

float4 PS_Main( vertexOutput IN) : color
{
	float3 final=tex2D(mainTexture,IN.texCoordDiffuse.xy).rgb;
	float depth=length(IN.wPosition-eyePosition)/200000.f;
    return float4(final,depth);
}

float4 PS_Grass( vertexOutput IN) : color
{
	float3 colour=tex2D(grass_texture,IN.texCoordDiffuse.xy).rgb;
    return float4(colour,IN.normal.a);
}

float4 PS_Shadow(vertexOutput IN) : color
{
	float3 colour=lightColour*saturate(dot(IN.normal.rgb,lightDir))+ambientColour;
    return float4(colour,1.f);
}

float4 PS_CloudShadow(vertexOutput IN) : color
{
	float3 cloud_texc=(float3(IN.wPosition.xz-cloudOffset.xy,0))*cloudScales;
	float4 cloud1=tex3D(cloud_texture1,cloud_texc);
	float4 cloud2=tex3D(cloud_texture2,cloud_texc);
	
	float light=lerp(cloud1.y,cloud2.y,cloudInterp);

	float3 colour=lightColour*light*saturate(dot(IN.normal.rgb,lightDir))+ambientColour;
    return float4(colour,1.f);
}

float4 PS_Outline( vertexOutput IN) : color
{
    return float4(1,1,1,0.5);
}

technique simul_terrain
{
    pass base 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();

		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
    pass shadow 
    {
		PixelShader = compile ps_2_0 PS_Shadow();
        AlphaBlendEnable = true;
        ZWriteEnable= false;
		SrcBlend	= Zero;
		DestBlend	= SrcColor;
		ZFunc = lessequal;
    }
#if 0
	pass outline
    {
        AlphaBlendEnable = true;
		PixelShader = compile ps_2_0 PS_Outline();
		SrcBlend	= One;
		DestBlend	= One;
		FillMode	= Wireframe;
    }
#endif
    pass cloud_shadow 
    {
		PixelShader = compile ps_2_0 PS_CloudShadow();
        AlphaBlendEnable = true;
        ZWriteEnable= false;
		SrcBlend	= Zero;
		DestBlend	= SrcColor;
		ZFunc = lessequal;
    }
}

technique simul_depth_only
{
    pass depth 
    {
		alphablendenable = false;
		ColorWriteEnable=0;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
}

technique simul_at
{
    pass grass 
    {
		PixelShader = compile ps_2_0 PS_Grass();
		alphablendenable = true;
        ZWriteEnable= false;
		SrcBlend	= SrcAlpha;
		DestBlend	= InvSrcAlpha;
    }
    pass outline
    {
        AlphaBlendEnable = true;
        ZWriteEnable= false;
        Lighting	= false;
		PixelShader = compile ps_2_0 PS_Outline();
		SrcBlend	= SrcAlpha;
		DestBlend	= One;
		FillMode	= Wireframe;
    }
}
technique simul_road
{
    pass base 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();

		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
}


