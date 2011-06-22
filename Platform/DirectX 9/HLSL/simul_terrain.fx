float4x4 worldViewProj	: WorldViewProjection;
float4x4 world			: World;

texture g_mainTexture;
sampler mainTexture = sampler_state
{
    Texture = <g_mainTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};
texture detailTexture;
sampler detail_texture = sampler_state
{
    Texture = <detailTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture cloudTexture1;
sampler3D cloud_texture1= sampler_state 
{
    Texture = <cloudTexture1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
#ifdef WRAP_CLOUDS
	AddressU = Wrap;
	AddressV = Wrap;
#else
	AddressU = Border;
	AddressV = Border;
    borderColor = 0xFFFFFF;
#endif
	AddressW = Clamp;
};
texture cloudTexture2;
sampler3D cloud_texture2= sampler_state 
{
    Texture = <cloudTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
#ifdef WRAP_CLOUDS
	AddressU = Wrap;
	AddressV = Wrap;
#else
	AddressU = Border;
	AddressV = Border;
    borderColor = 0xFFFFFF;
#endif
	AddressW = Clamp;
};

#ifndef MAX_FADE_DISTANCE_METRES
#define MAX_FADE_DISTANCE_METRES (300000.f)
#endif

//float morphFactor;
float4 eyePosition;
float4 lightDir;
float4 MieRayleighRatio;
float HazeEccentricity;
float3 cloudScales;	
float3 cloudOffset;
float3 lightColour;
float3 ambientColour;
float cloudInterp;
float fadeInterp;
#define pi (3.1415926536f)

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
    OUT.normal=saturate(IN.normal);
    return OUT;
}

float4 PS_Main( vertexOutput IN) : color
{
	float3 final=tex2D(mainTexture,IN.texCoordDiffuse.xy/5.f);
	float depth=length(IN.wPosition-eyePosition)/MAX_FADE_DISTANCE_METRES;
    return float4(final,depth);
}

float4 PS_Detail( vertexOutput IN) : color
{
	float3 final=tex2D(detail_texture,IN.texCoordDiffuse.xy);
	float depth=length(IN.wPosition-eyePosition)/MAX_FADE_DISTANCE_METRES;
    return float4(final,IN.normal.a);
}

float4 PS_Shadow(vertexOutput IN) : color
{
	float direct_light=saturate(dot(IN.normal.rgb,lightDir));
	float3 colour=lightColour*direct_light+ambientColour;
    return float4(colour,1.f);
}

float4 PS_CloudShadow(vertexOutput IN) : color
{
#ifdef Y_VERTICAL
	float3 lightVec=lightDir/lightDir.y;
	float dz=IN.wPosition.y-cloudOffset.z;
	float3 wPos=float3(IN.wPosition.xz-cloudOffset.xy,0);
	wPos.xy-=lightVec.xz*dz;
#else
	float3 lightVec=lightDir/lightDir.z;
	float dz=IN.wPosition.z-cloudOffset.z;
	float3 wPos=float3(IN.wPosition.xy-cloudOffset.xy,0);
	wPos.xy-=lightVec.xy*dz;
#endif

	float3 cloud_texc=wPos*cloudScales;
	float4 cloud1=tex3D(cloud_texture1,cloud_texc);
	float4 cloud2=tex3D(cloud_texture2,cloud_texc);
	
	float direct_light=saturate(dot(IN.normal.rgb,lightDir));
	float light=lerp(cloud1.z,cloud2.z,cloudInterp);

	float3 colour=lightColour*light*direct_light+ambientColour;
    return float4(colour.rgb,1.f);
}

float4 PS_Outline( vertexOutput IN) : color
{
    return float4(1.f,0.f,0.f,0.5f);
}

technique simul_terrain
{
    pass base 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();

		ColorWriteEnable=blue|red|green|alpha;
		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
    pass detail 
    {		
		VertexShader=compile vs_2_0 VS_Main();
		PixelShader =compile ps_2_0 PS_Detail();
		ColorWriteEnable=blue|red|green;

        AlphaBlendEnable=true;
        ZWriteEnable	=false;
		ZEnable			=true;
        Lighting		= false;
		SrcBlend		= SrcAlpha;
		DestBlend		= InvSrcAlpha;
		ZFunc			= lessequal;
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
    pass cloud_shadow 
    {
		PixelShader = compile ps_2_0 PS_CloudShadow();
        AlphaBlendEnable = true;
        ZWriteEnable= false;
		SrcBlend	= Zero;
		DestBlend	= SrcColor;
		ZFunc = lessequal;
    }
    pass outline
    {
        AlphaBlendEnable = true;
        ZWriteEnable= false;
        ZEnable		= true;
        Lighting	= false;
		PixelShader = compile ps_2_0 PS_Outline();
		SrcBlend	= SrcAlpha;
		DestBlend	= InvSrcAlpha;
		FillMode	= Wireframe;
		ZFunc		= lessequal;
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
		PixelShader = compile ps_2_0 PS_Detail();
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


