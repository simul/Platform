float4x4 worldViewProj : WorldViewProjection;

texture skyTexture1;

#ifdef USE_ALTITUDE_INTERPOLATION
sampler2D texture1 = sampler_state
#else
sampler1D texture1 = sampler_state
#endif
{
    Texture = <skyTexture1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

texture skyTexture2;
#ifdef USE_ALTITUDE_INTERPOLATION
sampler2D texture2 = sampler_state
#else
sampler1D texture2 = sampler_state
#endif
{
    Texture = <skyTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

texture starsTexture;
sampler2D stars_texture = sampler_state
{
    Texture = <starsTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Mirror;
};

texture flareTexture;
sampler flare_texture = sampler_state
{
    Texture = <flareTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};
texture planetTexture;
sampler planet_texture = sampler_state
{
    Texture = <planetTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};



texture fadeTexture;
#ifdef USE_ALTITUDE_INTERPOLATION
	sampler3D fade_texture = sampler_state
#else
	sampler2D fade_texture = sampler_state
#endif
{
    Texture = <fadeTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
	AddressW = Clamp;
};
texture fadeTexture2;
#ifdef USE_ALTITUDE_INTERPOLATION
	sampler3D fade_texture_2 = sampler_state
#else
	sampler2D fade_texture_2= sampler_state
#endif
{
    Texture = <fadeTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
	AddressW = Clamp;
};
texture crossSectionTexture;
sampler2D cross_section_texture= sampler_state
{
    Texture = <crossSectionTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

//------------------------------------
// Parameters 
//------------------------------------
float4 eyePosition : EYEPOSITION_WORLDSPACE;
float4 lightDir : Direction;
float4 mieRayleighRatio;
float hazeEccentricity;
float skyInterp;
#ifdef USE_ALTITUDE_INTERPOLATION
	float altitudeTexCoord=0;
#endif
#define pi (3.1415926536f)

float3 colour;
float starBrightness=.1f;
//------------------------------------
// Structures 
//------------------------------------
struct vertexInput
{
    float3 position			: POSITION;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
    float3 wDirection		: TEXCOORD1;
};

struct vertexInputCS
{
    float3 position			: POSITION;
    float4 colour			: COLOR;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    float4 hPosition		: POSITION;
    float3 texCoords		: TEXCOORD0;
    float4 colour			: TEXCOORD2;
};
struct vertexInput3Dto2D
{
    float3 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutput3Dto2D
{
    float4 hPosition		: POSITION;
    float2 texCoords		: TEXCOORD0;
};
//------------------------------------
// Vertex Shader 
//------------------------------------
vertexOutput VS_Main(vertexInput IN) 
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.hPosition.z=OUT.hPosition.w;
    OUT.wDirection=normalize(IN.position.xyz);
    return OUT;
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return 0.5*0.079577+0.5*(1.f-g2)/(4.f*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831f*(1.f+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 output=BetaTotal*inscatter_factor.rgb;
	return output;
}

float4 PS_Main( vertexOutput IN): color
{
	float3 view=normalize(IN.wDirection.xyz);
#ifdef Y_VERTICAL
	float sine	=view.y;
#else
	float sine	=view.z;
#endif
#ifdef USE_ALTITUDE_INTERPOLATION
	float2 texcoord	=float2(0.5f*(1.f-sine),altitudeTexCoord);//float2(0.5f*(1.f-sine),altitudeTexCoord);
	float4 inscatter_factor1=tex2D(texture1,texcoord);
	float4 inscatter_factor2=tex2D(texture2,texcoord);
#else
	float texcoord	=0.5f*(1.f-sine);
	float4 inscatter_factor1=tex1D(texture1,texcoord);
	float4 inscatter_factor2=tex1D(texture2,texcoord);
#endif
	float4 inscatter_factor=lerp(inscatter_factor1,inscatter_factor2,skyInterp);
	float cos0=dot(lightDir.xyz,view.xyz);
	float3 colour=InscatterFunction(inscatter_factor,cos0);
	return float4(colour,1.f);
}

float4 PS_Stars(vertexOutput IN): color
{
	float3 view=normalize(IN.wDirection.xyz);
#ifdef Y_VERTICAL
	float sine	=view.y;
	float azimuth=atan2(view.x,view.z);
#else
	float sine	=view.z;
	float azimuth=atan2(view.x,view.y);
#endif
	float elev=asin(sine);
	float2 stars_texcoord=float2(azimuth/(2.0*pi),0.5*(1.f-elev/(pi/2.0)));
	float3 colour=starBrightness*tex2D(stars_texture,stars_texcoord);
	return float4(colour,1.f);
}

struct svertexInput
{
    float3 position			: POSITION;
    float2 tex				: TEXCOORD0;
};

struct svertexOutput
{
    float4 hPosition		: POSITION;
    float2 tex				: TEXCOORD0;
};

svertexOutput VS_Sun(svertexInput IN) 
{
    svertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.tex=IN.position.xy;
    return OUT;
}

float4 PS_Sun(svertexOutput IN): color
{
	float r=length(IN.tex);
	float brightness;
	if(r<0.99f)
		brightness=(0.99f-r)/0.4f;
	else
		brightness=0.f;
	brightness=saturate(brightness);
	float3 output=brightness*colour;
	return float4(output,1.f);
}

float4 PS_Flare(svertexOutput IN): color
{
	float3 output=colour*tex2D(flare_texture,float2(0.5f,0.5f)+0.5f*IN.tex);
	return float4(output,1.f);
}

float4 PS_Planet(svertexOutput IN): color
{
	float4 output=tex2D(planet_texture,float2(0.5f,0.5f)-0.5f*IN.tex);
	// IN.tex is +- 1.
	float3 normal;
	normal.x=IN.tex.x;
	normal.y=IN.tex.y;
	float l=length(IN.tex);
	normal.z=-sqrt(1.f-l*l);
	float light=saturate(dot(normal.xyz,lightDir.xyz));
	output.rgb*=colour.rgb;
	output.rgb*=light;
	output.a=saturate((1.0-l)/0.01);
	return output;
}

float4 PS_Query(svertexOutput IN): color
{
	return float4(1.f,0.f,0.f,0.f);
}

svertexOutput VS_Point_Stars(svertexInput IN) 
{
    svertexOutput OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.tex=IN.tex;
    return OUT;
}

float4 PS_Point_Stars(svertexOutput IN): color
{
	float3 colour=float3(1.f,1.f,1.f);
	colour=colour*saturate(starBrightness*IN.tex.x);
	return float4(colour,1.f);
}

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
	return IN.colour;
}


vertexOutputCS VS_CrossSection(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition = mul(worldViewProj,float4(IN.position.xyz,1.0));
//float4(1,1,0,1.0);//
	OUT.texCoords.xy=IN.texCoords.xy;
	OUT.texCoords.z=1;
	OUT.colour=IN.colour;
    return OUT;
}

float4 PS_CrossSection( vertexOutputCS IN): color
{
#if 0//def USE_ALTITUDE_INTERPOLATION
	float3 texc=float3(IN.texCoords.x,IN.texCoords.y,altitudeTexCoord);
	float4 colour=tex3D(fade_texture,texc);
#else
	float4 colour=tex2D(cross_section_texture,IN.texCoords.xy);
#endif
	colour.rgb=pow(colour.rgb,0.45f);
    return float4(colour.rgb,1);
}

vertexOutput3Dto2D VS_3D_to_2D(vertexInput3Dto2D IN)
{
    vertexOutput3Dto2D OUT;
    OUT.hPosition =float4(IN.position,1.f);
	OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_3D_to_2D(vertexOutput3Dto2D IN): color
{
#ifdef USE_ALTITUDE_INTERPOLATION
	float3 texc=float3(IN.texCoords.x,IN.texCoords.y,altitudeTexCoord);
	float4 colour1=tex3D(fade_texture,texc);
	float4 colour2=tex3D(fade_texture_2,texc);
#else
	float2 texc=float2(IN.texCoords.x,IN.texCoords.y);
	float4 colour1=tex2D(fade_texture,texc);
	float4 colour2=tex2D(fade_texture_2,texc);
#endif
	float4 colour=lerp(colour1,colour2,skyInterp);
    return colour;
}

struct vertexInputPosTex
{
    float3 position			: POSITION;
    float4 texCoords		: TEXCOORD0;
};

struct vertexOutputPosTex
{
    float4 hPosition		: POSITION;
    float4 texCoords		: TEXCOORD0;
};

vertexOutputPosTex VS_PointStars(vertexInputPosTex IN)
{
    vertexOutputPosTex OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.hPosition.z=OUT.hPosition.w;
    OUT.texCoords=IN.texCoords;
    return OUT;
}

float4 PS_PointStars(vertexOutputPosTex IN): color
{
	float3 colour=starBrightness*float3(1.f,1.f,1.f)*IN.texCoords.x;
	return float4(colour,1.f);
}

//------------------------------------
// Technique 
//------------------------------------
technique simul_sky
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();
       
        CullMode = None;
		zenable = true;
		zwriteenable = false;
      //  AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_starry_sky
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_b PS_Stars();
       
        CullMode = None;
		zenable = true;
		zwriteenable = false;
        AlphaBlendEnable = false;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
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

technique simul_sun
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Sun();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_planet
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Planet();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_flare
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_Sun();
		PixelShader  = compile ps_2_0 PS_Flare();
       
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_query
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_Sun();
		PixelShader  = compile ps_3_0 PS_Query();
		zenable = true;
		zwriteenable = false;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}
technique simul_fade_cross_section
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_CrossSection();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_fade_3d_to_2d
{
    pass p0 
    {		
		VertexShader = compile vs_3_0 VS_3D_to_2D();
		PixelShader  = compile ps_3_0 PS_3D_to_2D();
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
#ifndef XBOX
		lighting = false;
#endif
    }
}

technique simul_point_stars
{
    pass p0 
    {		
		VertexShader = compile vs_2_0 VS_PointStars();
		PixelShader  = compile ps_2_b PS_PointStars();
       
        CullMode = None;
		zenable = false;
		zwriteenable = false;
        AlphaBlendEnable = false;
// Don't write alpha, as that's depth!
		ColorWriteEnable=7;
#ifndef XBOX
		lighting = false;
#endif
    }
}
