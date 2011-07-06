float4x4 worldViewProj	: WorldViewProjection;

// raytrace
float4x4 invViewProj;
float4x4 noiseMatrix;
float3 cloudScales;
float3 cloudOffset;

texture cloudDensity1;
sampler3D cloud_density_1= sampler_state 
{
    Texture = <cloudDensity1>;
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
float4 lightResponse;
float3 lightDir : Direction;
float3 skylightColour;
float4 fractalScale;
float interp;
float crossSectionOffset;
// Noise in raytracing:
float fractalRepeatLength;
// Lightning glow:
float4 lightningMultipliers;
float4 lightningColour;
float3 illuminationOrigin;
float3 illuminationScales;
float hazeEccentricity=0;
float3 mieRayleighRatio;
float fadeInterp=0;
float distance=1.0;
float3 cornerPos;
float3 texScales;
float layerFade;
float cloudEccentricity=0.87f;
float altitudeTexCoord=0;
float alphaSharpness=0.5f;
#ifndef MAX_FADE_DISTANCE_METRES
	#define MAX_FADE_DISTANCE_METRES (200000.f)
#endif

struct vertexInput
{
    float3 position			: POSITION;
    float3 texCoords		: TEXCOORD0;
    float layerFade			: TEXCOORD1;
    float2 texCoordsNoise	: TEXCOORD2;
    float3 lightColour		: TEXCOORD3;
#if FADE_MODE==0
    float3 loss				: TEXCOORD4;
    float3 inscatter		: TEXCOORD5;
#endif
};

struct vertexInputNew
{
    float3 position			: POSITION;
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
#if FADE_MODE==1
    float2 fade_texc			: TEXCOORD6;
#endif
#if FADE_MODE==0
    float3 loss					: TEXCOORD6;
    float3 inscatter			: TEXCOORD7;
#endif
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	
	OUT.texCoords.xyz=IN.texCoords;
	OUT.texCoords.w=0.5f+0.5f*saturate(IN.texCoords.z);
	const float c=fractalScale.w;
	OUT.texCoordsNoise=IN.texCoordsNoise;
	OUT.wPosition=(IN.position.xyz-eyePosition.xyz);
	OUT.layerFade=IN.layerFade;
	// Note position.xzy is used!
	float3 texCoordLightning=(IN.position.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
	texCoordLightning.z=0.5f;
	OUT.texCoordLightning=texCoordLightning;
	float3 view=normalize(OUT.wPosition.xyz);
#ifdef Z_VERTICAL
	float sine	=view.z;
#else
	float sine	=view.y;
#endif
	OUT.lightColour=IN.lightColour;
// Fade mode ZERO - fade values come from the vertex. So we pass them on to the pixel shader:
#if FADE_MODE==0
    OUT.loss			=IN.loss;
    OUT.inscatter		=IN.inscatter;
#endif
// Fade mode ONE - fade is calculated from the fade textures. So we send a texture coordinate:
#if FADE_MODE==1
	OUT.fade_texc=float2(length(OUT.wPosition.xyz)/MAX_FADE_DISTANCE_METRES,0.5f*(1.f-sine));
#endif
    return OUT;
}

#define pi (3.1415926536f)
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
		/(float3(1.f,1.f,1.f)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

float4 PS_WithLightning(vertexOutput IN): color
{
	float3 noise_offset=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float3 noiseval=tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-noise_offset;
#ifdef DETAIL_NOISE
	noiseval+=(tex2D(noise_texture,IN.texCoordsNoise.xy*8).xyz-noise_offset)/2.0;
	noiseval*=IN.texCoords.w;
#endif
	noiseval*=IN.texCoords.w;
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=tex3D(cloud_density_1,pos);
	float4 density2=tex3D(cloud_density_2,pos);

	density=lerp(density,density2,interp);

	density.x*=IN.layerFade;
	density.x=saturate(density.x*1.5f-0.5f);

	if(density.x<=0)
		discard;

	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
#if FADE_MODE==1
	float3 loss=tex2D(sky_loss_texture,IN.fade_texc).rgb;
	float4 insc=tex2D(sky_inscatter_texture,IN.fade_texc);
	float3 inscatter=InscatterFunction(insc,cos0);
#endif
#if FADE_MODE==0
	float3 loss=IN.loss;
	float3 inscatter=IN.inscatter;
#endif
	float4 lightning=tex3D(lightning_illumination,IN.texCoordLightning.xyz);

	float3 ambient=density.w*skylightColour.rgb;
	float opacity=density.x;
	float l=dot(lightningMultipliers.xyzw,lightning.xyzw);
	float3 lightningC=l*lightningColour.xyz;
	float3 final=(lightResponse.x*density.z*Beta+lightResponse.y*density.y)*IN.lightColour+ambient.rgb;
	final+=lightningColour.w*lightningC;

	final*=loss.xyz;
	final+=inscatter.xyz;
	final*=opacity;

	final+=lightningC*(opacity+IN.layerFade);
    return float4(final.rgb,opacity);
}

float4 PS_Clouds( vertexOutput IN): color
{
	float3 noise_offset=float3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float3 noiseval=tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-noise_offset;
#ifdef DETAIL_NOISE
	noiseval+=(tex2D(noise_texture,IN.texCoordsNoise.xy*8).xyz-noise_offset)/2.0;
	noiseval*=IN.texCoords.w;
#endif
	float3 texcoord=IN.texCoords.xyz+fractalScale.xyz*noiseval;

	float4 density=tex3D(cloud_density_1,texcoord);
	float4 density2=tex3D(cloud_density_2,texcoord);

	density=lerp(density,density2,interp);

	density.x*=IN.layerFade;
	density.x=saturate(density.x*(1.f+alphaSharpness)-alphaSharpness);

	if(density.x<=0)
		discard;
	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
// cloudEccentricity is multiplied by density.z (i.e. direct light) to avoid interpolation artifacts.
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
// Fade mode 1 means using textures for distance fade.
#if FADE_MODE==1
	float4 insc=tex2D(sky_inscatter_texture,IN.fade_texc);
	float3 loss=tex2D(sky_loss_texture,IN.fade_texc).rgb;
	float3 inscatter=InscatterFunction(insc,cos0);
#endif
// Fade mode 0 means passing fade values in through vertex properties.
#if FADE_MODE==0
	float3 loss=IN.loss;
	float3 inscatter=IN.inscatter;
#endif
	float3 ambient=skylightColour.rgb*density.w;

	float opacity=density.x;
	float3 final=(density.z*Beta+lightResponse.y*density.y)*IN.lightColour+ambient.rgb;

	final*=loss;
	final+=inscatter;

	final*=opacity;
    return float4(final,opacity);
}

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

vertexOutputCS VS_CrossSection(vertexInputCS IN)
{
    vertexOutputCS OUT;
    OUT.hPosition = float4(1,1,0,1.0);//mul(worldViewProj,float4(IN.position.xyz,1.0));

	OUT.texCoords.xy=float2(1,1);
	OUT.texCoords.z=1;
	OUT.colour=IN.colour;
    return OUT;
}

float4 PS_CrossSectionXZ( vertexOutputCS IN): color
{
	float3 texc=float3(crossSectionOffset+IN.texCoords.x,0,1.-IN.texCoords.y);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	for(i=0;i<32;i++)
	{
		texc.y=i/32.f;
		float4 density=tex3D(cloud_density_1,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.z+lightResponse.y*density.y);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x+.05f;
		colour*=opacity;
		accum*=0.9998f;
		accum*=1.f-opacity;
		accum+=colour;
	}
    return float4(accum,1);
}

float4 PS_CrossSectionXY( vertexOutputCS IN): color
{
	float3 texc=float3(crossSectionOffset+IN.texCoords.x,crossSectionOffset+IN.texCoords.y,0.125);
	int i=0;
	float3 accum=float3(0.f,0.5f,1.f);
	for(i=0;i<16;i++)
	{
		texc.z=i/16.f;
		float4 density=tex3D(cloud_density_1,texc);
		float3 colour=float3(.5,.5,.5)*(lightResponse.x*density.z+lightResponse.y*density.y);
		colour.gb+=float2(.125,.25)*(lightResponse.z*density.w);
		float opacity=density.x+.05f;
		colour*=opacity;
		accum*=0.9998f;
		accum*=1.f-opacity;
		accum+=colour;
	}
    return float4(accum,1);
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
#ifdef Y_VERTICAL
	float sine	=view.y;
	float2 base_noise_texc=float2(atan2(noise_pos.x,noise_pos.y),atan2(noise_pos.z,noise_pos.y));
#else
	float sine	=view.z;
	float2 base_noise_texc=float2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));
#endif
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
	//	if(d==0)
	//		continue;
		//d*=d;
		//float dd=depth*10.f-d;
		//d*=200000.0;
		wPosition*=d;
#if FADE_MODE==1
		fade_texc.x=d/MAX_FADE_DISTANCE_METRES;
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
#ifdef Y_VERTICAL
		float3 cloud_texc=(wPosition.xzy-cloudOffset.xyz)*cloudScales.xyz;
#else
		float3 cloud_texc=(wPosition.xyz-cloudOffset.xyz)*cloudScales.xyz;
#endif
#ifdef DETAIL_NOISE
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
		SrcBlend = One;
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

// If shader model 3 is not available, 2 will do, but is slower on SM3-capable hardware!
technique simul_clouds_sm2
{
    pass p0 
    {
		zenable = true;
		zfunc = lessequal;
		DepthBias =0;
		SlopeScaleDepthBias =0;
		ZWriteEnable = false;
		alphablendenable = true;
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

		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Clouds();
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

