float4x4 worldViewProj	: WorldViewProjection;
float4x4 world			: World;

texture heightTexture;
sampler height_texture = sampler_state
{
    Texture = <heightTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

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

texture waterTexture;
sampler water_texture = sampler_state
{
    Texture = <waterTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
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

texture colourkeyTexture;
sampler1D colourkey_texture=sampler_state
{
    Texture = <colourkeyTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
};

#ifndef MAX_FADE_DISTANCE_METRES
#define MAX_FADE_DISTANCE_METRES (300000.f)
#endif
float mipLevels;
float2 morphFactor;
float4 eyePosition;
float4 lightDir;
float3 cloudScales;	
float3 cloudOffset;
float3 lightColour;
float3 ambientColour;
float cloudInterp;
float2 texOffsetH; // offset in heightmap for texcoords

float altitudeBase=0.0;
float altitudeRange=1000.0;

#define pi (3.1415926536f)

struct mapVertexInput
{
    float3 position			: POSITION;
    float2 texCoordDiffuse	: TEXCOORD0;
};

struct mapVertexOutput
{
    float4 hPosition		: POSITION;
    float2 texCoordDiffuse	: TEXCOORD0;
};

mapVertexOutput VS_Map(mapVertexInput IN)
{
    mapVertexOutput OUT;
    OUT.hPosition = mul(worldViewProj,float4(IN.position.xyz,1.f));
    OUT.texCoordDiffuse=IN.texCoordDiffuse;
    return OUT;
}

float4 PS_Map(mapVertexOutput IN) : color
{
	float H=(tex2D(height_texture,IN.texCoordDiffuse.xy).r-altitudeBase)/altitudeRange;
	float3 colour=tex1D(colourkey_texture,saturate(H)).rgb;
	float4 W=tex2D(water_texture,IN.texCoordDiffuse.xy);
	float3 water_colour=float3(0.2,0.7,1.0);
	water_colour=lerp(water_colour,float3(1.0,0.0,0.0),saturate(W.z/(W.w+0.001)));
	colour=lerp(colour,water_colour,saturate(W.w/10.0));
    return float4(colour.rgb,1.f);
}

struct vertexInput
{
    float2 position			: POSITION;
    float2 texCoordDiffuse	: TEXCOORD1;
    float2 morph			: TEXCOORD2;
    float2 texCoordH		: TEXCOORD3;
    float2 texCoordH1		: TEXCOORD4;
    float2 texCoordH2		: TEXCOORD5;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
    float2 texCoordDiffuse	: TEXCOORD1;
    float4 wPosition		: TEXCOORD2;
    float2 heightmap_texc	: TEXCOORD3;
    float2 test				: TEXCOORD4;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
#ifdef Y_VERTICAL
	float3 position=mul(float4(IN.position.x,0,IN.position.y,1.0),world);
#else
	float3 position=mul(float4(IN.position.xy,0.0,1.0),world);
#endif
	float2 heightmap_texc=IN.texCoordH+texOffsetH;
	float2 heightmap_texc_1=IN.texCoordH1+texOffsetH;
	float2 heightmap_texc_2=IN.texCoordH2+texOffsetH;
	float m=IN.morph.y*(lerp(morphFactor.x,morphFactor.y,IN.morph.x));
	float mip=mipLevels*m;
//	heightmap_texc=heightmap_texc+pow(2.0,mip)/512.0;
	float h=tex2Dlod(height_texture,float4(heightmap_texc,0,0)).r+tex2Dlod(water_texture,float4(heightmap_texc,0,0)).w;
	float h1=tex2Dlod(height_texture,float4(heightmap_texc_1,0,0)).r+tex2Dlod(water_texture,float4(heightmap_texc_1,0,0)).w;
	float h2=tex2Dlod(height_texture,float4(heightmap_texc_2,0,0)).r+tex2Dlod(water_texture,float4(heightmap_texc_2,0,0)).w;
	OUT.test.g=frac(mip);
	h=lerp(h,0.5*(h1+h2),OUT.test.g);
	OUT.test.r=h;
#ifdef Y_VERTICAL
	position.y=OUT.test.r;
#else
	position.z=OUT.test.r;
#endif
    OUT.hPosition=mul(worldViewProj,float4(position.xyz,1.f));
    OUT.wPosition=float4(position.xyz,1.f);
    OUT.texCoordDiffuse=IN.texCoordDiffuse;
	OUT.heightmap_texc=heightmap_texc;
    return OUT;
}

float4 PS_Main( vertexOutput IN) : color
{
	float2 height_soil=tex2D(height_texture,IN.heightmap_texc.xy).xw;
	float3 final=tex2D(mainTexture,IN.texCoordDiffuse.xy);
	final=lerp(float3(0.5,0.45,0.3),final,saturate(height_soil.y/1.0));
	float4 water_texel=tex2D(water_texture,IN.heightmap_texc.xy);
	float3 water_colour=lerp(float3(0.1,0.2,0.5),float3(1.0,1.0,1.0),saturate(0.1*water_texel.x));
	water_colour=lerp(water_colour,float3(1.0,0.5,0.0),saturate(water_texel.z/10.0));
	final=lerp(final,water_colour,saturate(water_texel.w/10.0));
	float depth=length(IN.wPosition-eyePosition)/MAX_FADE_DISTANCE_METRES;
    return float4(final,depth);
}

float4 PS_Detail( vertexOutput IN) : color
{
	float2 height_soil=tex2D(height_texture,IN.heightmap_texc.xy).xw;
	float3 final=tex2D(detail_texture,IN.texCoordDiffuse.xy);
	final=lerp(float3(0.5,0.45,0.3),final,saturate(height_soil.y/1.0));
	float4 water_texel=tex2D(water_texture,IN.heightmap_texc.xy);
	float3 water_colour=lerp(float3(0.1,0.2,0.5),float3(1.0,1.0,1.0),saturate(0.1*water_texel.x));
	water_colour=lerp(water_colour,float3(1.0,0.5,0.0),saturate(water_texel.z/10.0));
	final=lerp(final,water_colour,saturate(water_texel.w/10.0));
	float depth=length(IN.wPosition-eyePosition)/MAX_FADE_DISTANCE_METRES;
    return float4(final,0.f);
}

float4 PS_Shadow(vertexOutput IN) : color
{
	float3 normal;
#ifdef Y_VERTICAL
	normal.xz=tex2D(height_texture,IN.heightmap_texc.xy).yz;
	normal.y=1.0-sqrt(dot(normal.xz,normal.xz));
#else
	normal.xy=tex2D(height_texture,IN.heightmap_texc.xy).yz;
	normal.z=1.0-sqrt(dot(normal.xy,normal.xy));
#endif
	float direct_light=saturate(dot(normal.xyz,lightDir));
	float3 colour=lightColour*direct_light+ambientColour;
    return float4(colour,1.f);
}

float4 PS_CloudShadow(vertexOutput IN) : color
{
	float3 normal;
#ifdef Y_VERTICAL
	normal.xz=tex2D(height_texture,IN.heightmap_texc.xy).yz;
	normal.y=1.0-sqrt(dot(normal.xz,normal.xz));
	float3 lightVec=lightDir/lightDir.y;
	float dz=IN.wPosition.y-cloudOffset.z;
	float3 wPos=float3(IN.wPosition.xz-cloudOffset.xy,0);
	wPos.xy-=lightVec.xz*dz;
#else
	normal.xy=tex2D(height_texture,IN.heightmap_texc.xy).yz;
	normal.z=1.0-sqrt(dot(normal.xy,normal.xy));
	float3 lightVec=lightDir/lightDir.z;
	float dz=IN.wPosition.z-cloudOffset.z;
	float3 wPos=float3(IN.wPosition.xy-cloudOffset.xy,0);
	wPos.xy-=lightVec.xy*dz;
#endif
	float3 cloud_texc=wPos*cloudScales;
	float4 cloud1=tex3D(cloud_texture1,cloud_texc);
	float4 cloud2=tex3D(cloud_texture2,cloud_texc);
	float direct_light=saturate(dot(normal.xyz,lightDir));
	float light=lerp(cloud1.z,cloud2.z,cloudInterp);
	float3 colour=lightColour*light*direct_light+ambientColour;
    return float4(colour.rgb,1.f);
}

float4 PS_Outline( vertexOutput IN) : color
{
    return float4(lightColour,0.5f);
}

technique simul_terrain
{
    pass base 
    {		
		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Main();

		ColorWriteEnable=blue|red|green|alpha;
		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
    pass detail
    {		
		VertexShader=compile vs_3_0 VS_Main();
		PixelShader =compile ps_3_0 PS_Detail();
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
		VertexShader=compile vs_3_0 VS_Main();
		PixelShader = compile ps_3_0 PS_Shadow();
        AlphaBlendEnable = true;
        ZWriteEnable= false;
		SrcBlend	= Zero;
		DestBlend	= SrcColor;
		ZFunc = lessequal;
    }
    pass cloud_shadow
    {
		VertexShader=compile vs_3_0 VS_Main();
		PixelShader = compile ps_3_0 PS_CloudShadow();
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
		VertexShader=compile vs_3_0 VS_Main();
		PixelShader = compile ps_3_0 PS_Outline();
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
		PixelShader = compile ps_3_0 PS_Detail();
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
		PixelShader = compile ps_3_0 PS_Outline();
		SrcBlend	= SrcAlpha;
		DestBlend	= One;
		FillMode	= Wireframe;
    }
}
technique simul_road
{
    pass base
    {		
		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Main();

		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
}

technique simul_map
{
    pass base
    {		
		VertexShader = compile vs_3_0 VS_Map();
		PixelShader  = compile ps_3_0 PS_Map();

		alphablendenable = false;
        ZWriteEnable = true;
		ZEnable = true;
		ZFunc = less;
    }
}
