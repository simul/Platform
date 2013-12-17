#include "dx9.hlsl"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#ifndef WRAP_CLOUDS
	#define WRAP_CLOUDS 1
#endif
#ifndef DETAIL_NOISE
	#define DETAIL_NOISE 1
#endif
#ifndef FADE_MODE
	#define FADE_MODE 1
#endif

float4x4 worldViewProj	: WorldViewProjection;

// raytrace
float3 cloudScales;
float3 cloudOffset;

texture cloudDensity1;
sampler3D cloud_density_1= sampler_state 
{
    Texture = <cloudDensity1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
	SRGBTexture = 0;
};
sampler3D cloud_density_1a= sampler_state 
{
    Texture = <cloudDensity1>;
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
	SRGBTexture = 0;
};

texture cloudDensity2;
sampler3D cloud_density_2= sampler_state 
{
    Texture = <cloudDensity2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
#ifdef WRAP_CLOUDS
	AddressU = Wrap;
	AddressV = Wrap;
#else
	AddressU = Clamp;
	AddressV = Clamp;
#endif
	AddressW = Clamp;
	SRGBTexture = 0;
};

texture noiseTexture;
sampler2D noise_texture= sampler_state 
{
    Texture = <noiseTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
	SRGBTexture = 0;
};

texture lightningIlluminationTexture;
sampler3D lightning_illumination= sampler_state
{
    Texture = <lightningIlluminationTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Border;
	AddressV = Border;
	AddressW = Border;
	SRGBTexture = 0;
};
texture skyLossTexture;
sampler2D sky_loss_texture= sampler_state 
{
    Texture = <skyLossTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
	SRGBTexture = 0;
};
texture skyInscatterTexture;
sampler2D sky_inscatter_texture= sampler_state 
{
    Texture = <skyInscatterTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
	SRGBTexture = 0;
};
texture skylightTexture;
sampler2D skylight_texture= sampler_state 
{
    Texture = <skylightTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
	SRGBTexture = 0;
};
texture illuminationTexture;
sampler2D illumination_texture= sampler_state
{
    Texture = <illuminationTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
	SRGBTexture = 0;
};

texture raytraceLayerTexture;
sampler1D raytrace_layer_texture= sampler_state 
{
    Texture = <raytraceLayerTexture>;
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
	AddressU = Clamp;
	SRGBTexture = 0;
};

float4 eyePosition : EYEPOSITION_WORLDSPACE;
// Light response: primary,secondary,anisotropic,ambient
float3 lightDir : Direction;
float3 skylightColour;
float interp;
// Lightning glow:
float fadeInterp=0;
float distance=1.0;
float3 texScales;

struct vertexInput
{
    float3 position			: POSITION;
    vec2 layerNoiseOffset	: TEXCOORD0;
    float layerFade			: TEXCOORD1;
    float layerWorldDist	: TEXCOORD2;
};

struct vertexInputPositionColour
{
    float3 position			: POSITION;
    float4 colour			: TEXCOORD0;
    float2 texc				: TEXCOORD1;
};
struct vertexOutputPositionColour
{
    float4 hPosition		: POSITION;
    float4 colour			: TEXCOORD0;
    float2 texc				: TEXCOORD1;
};

struct vertexOutput
{
    float4 hPosition			: POSITION;
    float2 texCoordsNoise		: TEXCOORD0;
	float layerFade				: TEXCOORD1;
    float4 texCoords			: TEXCOORD2;
	float3 wPosition			: TEXCOORD3;
    float3 texCoordLightning	: TEXCOORD4;
    float3 lightColour			: TEXCOORD5;
    float2 fade_texc			: TEXCOORD6;
};

vertexOutputPositionColour VS_PositionColour(vertexInputPositionColour IN)
{
	vertexOutputPositionColour OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz,1.0));
	OUT.colour=IN.colour;
	OUT.texc=IN.texc;
	return OUT;
}
float4 PS_PositionColour(vertexOutputPositionColour IN): color
{
	return IN.colour;
}

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
	vec3 wPos				=IN.position.xyz+eyePosition.xyz;
    OUT.hPosition			=mul(worldViewProj, float4(wPos.xyz , 1.0));
//	wPos.z	-=wPos.z;
	OUT.texCoords.xyz		=(wPos-cornerPos.xyz)*inverseScales;
	OUT.texCoords.w			=0.5f+0.5f*saturate(OUT.texCoords.z);
	//const float c			=fractalScale.w;
	vec3 n					=vec3(OUT.hPosition.xy/OUT.hPosition.z*tanHalfFov,1.0);
	n						=normalize(n);
	vec2 noise_texc_0		=mul(noiseMatrix,vec4(n.xy,0,0)).xy/fractalRepeatLength;
	OUT.texCoordsNoise		=noise_texc_0*IN.layerWorldDist+IN.layerNoiseOffset;
	OUT.wPosition			=(wPos.xyz);//-eyePosition.xyz);
	OUT.layerFade=IN.layerFade;
// Note position.xzy is used if Y is vertical!
	float3 texCoordLightning=(wPos.xyz-illuminationOrigin.xyz)/illuminationScales.xyz;
	texCoordLightning.z=0.5f;
	OUT.texCoordLightning=texCoordLightning;
	float3 view=normalize(OUT.wPosition.xyz);
	float sine	=view.z;
	OUT.lightColour			=sunlightColour1;
// Fade mode ONE - fade is calculated from the fade textures. So we send a texture coordinate:
	float dist				=length(OUT.wPosition.xyz)/maxFadeDistanceMetres;
	//OUT.fade_texc=float2(,0.5f*(1.f-sine));
	OUT.fade_texc			=float2(sqrt(dist),0.5f*(1.f-sine));
    return OUT;
}

float4 CloudColour(vertexOutput IN,float cos0)
{
	float3 noise_offset=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float3 noiseval=tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-noise_offset;
#if DETAIL_NOISE==1
	//noiseval+=(tex2D(noise_texture,IN.texCoordsNoise.xy*8.0).xyz-noise_offset)/2.0;
#endif
	noiseval*=IN.texCoords.w;
	float3 texcoord=IN.texCoords.xyz+fractalScale.xyz*noiseval;

	float4 density=tex3D(cloud_density_1,texcoord);
	float4 density2=tex3D(cloud_density_2,texcoord);

	density=lerp(density,density2,interp);

	density.x*=IN.layerFade;
	density.x=saturate(density.x*(1.f+alphaSharpness)-alphaSharpness);

	//if(density.x<=0)
	//	discard;
// cloudEccentricity is multiplied by density.z (i.e. direct light) to avoid interpolation artifacts.
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.y,cos0);
	float3 ambient=skylightColour.rgb*density.w;
	float opacity=density.x;
	float4 final;
	final.rgb=(density.y*Beta+lightResponse.y*density.z)*IN.lightColour+ambient.rgb;
	final.a=opacity;
	return final;
}

float4 PS_WithLightning(vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec4 lookup=tex3D(cloud_density_1,IN.texCoords.xyz);
	return vec4(frac(IN.texCoords.xyz),.5);
	float4 final=CloudColour(IN,cos0);
	float opacity=final.a;
	float4 lightning=tex3D(lightning_illumination,IN.texCoordLightning.xyz);
	float l=dot(lightningMultipliers.xyzw,lightning.xyzw);
	float3 lightningC=l*lightningColour.xyz;
	final.rgb+=lightningColour.w*lightningC;
	float4 insc=tex2D(sky_inscatter_texture,IN.fade_texc);
	float3 loss=tex2D(sky_loss_texture,IN.fade_texc).rgb;
	float3 skyl=tex2D(skylight_texture,IN.fade_texc).rgb;
	float3 inscatter=skyl+InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	final.rgb*=loss.xyz;
	final.rgb+=inscatter.xyz;
	final.rgb*=opacity;

	final.rgb+=lightningC*(opacity+IN.layerFade);
    return float4(final.rgb,opacity);
}

vec4 PS_Clouds(vertexOutput IN): color
{
	vec3 view			=normalize(IN.wPosition);
	float sine			=view.z;
	float cos0=dot(lightDir.xyz,view.xyz);
// Fade mode 1 means using textures for distance fade.
	vec4 final			=CloudColour(IN,cos0);
	float opacity=final.a;
	vec2 fade_texc		=vec2(IN.fade_texc.x,0.5*(1.0-sine));
	vec4 insc			=tex2D(sky_inscatter_texture,fade_texc);
	vec3 loss			=tex2D(sky_loss_texture,fade_texc).rgb;
	vec3 skyl			=tex2D(skylight_texture,fade_texc).rgb;
	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illumination_texture,illum_texc).xy;
	float sh			=saturate((fade_texc.x-nearFarTexc.x)/0.1);
	vec3 inscatter		=skyl+sh*InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	final.rgb*=loss;
	final.rgb+=inscatter;
    return vec4(final.rgb,opacity);
}

struct vertexInputCS
{
    float3 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct vertexOutputCS
{
    float4 hPosition		: POSITION;
    float3 texCoords		: TEXCOORD0;
};

vertexOutputCS VS_CrossSection(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition = mul(worldViewProj,float4(IN.position.xyz,1.0));

	OUT.texCoords.xy=IN.texCoords;
	OUT.texCoords.z=1;
    return OUT;
}

vec4 PS_ShowNoise(vertexOutputCS IN): color
{
	return tex2D(noise_texture,IN.texCoords.xy);
}

#define CROSS_SECTION_STEPS 32
float4 PS_CrossSectionXZ( vertexOutputCS IN): color
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,0,crossSectionOffset.z+(1.0-IN.texCoords.y));
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.y+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=tex3D(cloud_density_1a,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.z);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.y+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
}

float4 PS_CrossSectionXY( vertexOutputCS IN): color
{
	float3 texc=float3(crossSectionOffset.x+IN.texCoords.x,crossSectionOffset.y+IN.texCoords.y,0);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	texc.z+=.5f/(float)CROSS_SECTION_STEPS;
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		float4 density=tex3D(cloud_density_1a,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.y+lightResponse.y*density.z);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x;
		colour*=opacity;
		accum*=1.f-opacity;
		accum+=colour;
		texc.z+=1.f/(float)CROSS_SECTION_STEPS;
	}
    return float4(accum,1);
}

float4 PS_RenderTo2D( vertexOutputCS IN): color
{
	float c=IN.texCoords.y*8.0;
	float3 texc=float3(IN.texCoords.x,0,0);
	float4 density=tex3D(cloud_density_1,texc);
	density.b=0;
    return density;
}

float4 PS_Mask( vertexOutput IN): color
{
	float3 noiseval=tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	
	float3 texcoord=IN.texCoords.xyz+fractalScale.xyz*noiseval;

	float4 density=tex3D(cloud_density_1,texcoord);
	float4 density2=tex3D(cloud_density_2,texcoord);

	density=lerp(density,density2,interp);

	//density.x*=IN.layerFade;
	//density.x=saturate(density.x*(1.f+alphaSharpness)-alphaSharpness);

    return float4(0,1,0,density.x);
}

struct raytraceVertexInput
{
    float2 position			: POSITION;
    float3 texCoords		: TEXCOORD0;
};

struct raytraceVertexOutput
{
    float4 position			: POSITION;
    float4 hpos_duplicate	: TEXCOORD0;
    float3 texCoords		: TEXCOORD1;
};

raytraceVertexOutput VS_Raytrace(raytraceVertexInput IN)
{
	raytraceVertexOutput OUT;
	OUT.position=float4(IN.position.xy,0,1);
	OUT.hpos_duplicate=float4(IN.position.xy,0,1);
	OUT.texCoords.xyz = IN.texCoords.xyz;
	return OUT;
}

float4 PS_RaytraceWithLightning(raytraceVertexOutput IN) : color
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y-=2.f*IN.texCoords.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float depth=1.f;
	float light=0;
	float filter=1.f;
	float4 colour=float4(0,0,0,1.0);
	float3 noise_pos=mul(noiseMatrix,float4(view,1.0f)).xyz;
	float3 noise_offset=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);

	float sine	=view.z;
	float2 base_noise_texc=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));

	float cos0=dot(lightDir.xyz,view.xyz);
// Fade mode ONE - fade is calculated from the fade textures. So we send a texture coordinate:
#if FADE_MODE==1
	float2 fade_texc=float2(0,0.5f*(1.f-sine));
	//OUT.fade_texc=float3(.5f,0.5f,0.5f);
#endif
	float3 lightColour=float3(25,25,25);
	for(int i=0;i<128;i++)
	{
		float3 wPosition=view;
		float d=(128-i-0.5)/128.0;
		float4 layer_lookup=tex1D(raytrace_layer_texture,d);
		d=layer_lookup.x;

		wPosition*=d;
#if FADE_MODE==1
		fade_texc.x=d/maxFadeDistanceMetres;
#endif
		wPosition+=eyePosition.xyz;

		float2 noise_texc=base_noise_texc;
		// How often the noise texture repeates:
		float noiseScale=d/fractalRepeatLength;
			// How much the noise texture offsets the lookup:
		float2 noiseOffset=layer_lookup.yz/fractalRepeatLength;
		noise_texc*=noiseScale;
		noise_texc+=noiseOffset;
		float3 noiseval=tex2D(noise_texture,noise_texc.xy).xyz-noise_offset;

		float3 cloud_texc=(wPosition.xyz-cloudOffset.xyz)*cloudScales.xyz;

#if DETAIL_NOISE==1
		noiseval+=(tex2D(noise_texture,noise_texc.xy*8).xyz-noise_offset)/2.0;
		noiseval*=0.5+0.5*cloud_texc.z;
#endif
		cloud_texc+=fractalScale.xyz*noiseval;
	//	if(cloud_texc.z>1.0||cloud_texc.z<0)
	//		continue;
		float4 cloud1=tex3D(cloud_density_1,cloud_texc);
		float4 cloud2=tex3D(cloud_density_2,cloud_texc);
		float4 cloud=lerp(cloud1,cloud2,interp);
		float3 ambient=cloud.w*skylightColour.rgb;
		float opacity=cloud.x;
		float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*cloud.z,cos0);
		float3 additional=(lightResponse.x*cloud.z*Beta+lightResponse.y*cloud.y)*lightColour+ambient.rgb;
/*
		// Fade mode 1 means using textures for distance fade.
#if FADE_MODE==1
		float3 loss=tex2D(sky_loss_texture,fade_texc).rgb;
		additional*=loss.xyz;
		float4 insc=tex2D(sky_inscatter_texture,fade_texc);
		float3 inscatter=InscatterFunction(insc,cos0);
		additional+=inscatter.xyz;
#endif*/
		additional*=opacity;
		colour*=(1.f-cloud.x);
		colour.rgb+=additional;
	}	
    return float4(colour.rgb,1.0-colour.a);
}

technique cloud_mask
{
    pass p0 
    {
		zenable = true;
		DepthBias =0;
		//AlphaTestEnable= true;
		AlphaFunc = greater;
		AlphaRef = 0.5;
		SlopeScaleDepthBias =0;
		ZWriteEnable = true;
		AlphaBlendEnable = false;
		COLORWRITEENABLE=0;
        CullMode = None;
		FillMode = Solid;

		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Mask();
    }
}
technique simul_clouds
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias =0;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Clouds();
    }
}
technique cross_section_xz
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias=0;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;
		COLORWRITEENABLE=15;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_CrossSectionXZ();
    }
}
technique cross_section_xy
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias=0;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;
		COLORWRITEENABLE=15;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_CrossSectionXY();
    }
}

technique simul_clouds_and_lightning
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias =0;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_WithLightning();
    }
}


technique raytrace_clouds_and_lightning
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias =0;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = InvSrcAlpha;
		
		// We would LIKE to do the following:
		//SeparateAlphaEnable = true;
		//SrcBlendAlpha = Zero;
		//DestBlendAlpha = InvSrcAlpha;
		// but it's not implemented!

		VertexShader = compile vs_3_0 VS_Raytrace();
		PixelShader  = compile ps_3_0 PS_RaytraceWithLightning();
    }
}

technique render_to_2d_for_saving
{
    pass p0 
    {
		zenable = false;
		zfunc = lessequal;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = false;

		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_RenderTo2D();
    }
}

technique colour_lines
{
    pass p0 
    {
		zenable = false;
		zfunc = lessequal;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = true;

		VertexShader = compile vs_3_0 VS_PositionColour();
		PixelShader  = compile ps_3_0 PS_PositionColour();
    }
}

technique show_noise
{
    pass p0 
    {
		zenable = false;
		zfunc = lessequal;
		ZWriteEnable = false;
        CullMode = None;
		AlphaTestEnable=false;
		FillMode = Solid;
        AlphaBlendEnable = false;

		VertexShader = compile vs_3_0 VS_CrossSection();
		PixelShader  = compile ps_3_0 PS_ShowNoise();
    }
}