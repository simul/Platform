float4x4 worldViewProj	: WorldViewProjection;

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
};

texture cloudDensity2;
sampler3D cloud_density_2= sampler_state 
{
    Texture = <cloudDensity2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Clamp;
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
};

texture largeScaleCloudTexture;
sampler2D large_scale_cloud_texture= sampler_state 
{
    Texture = <largeScaleCloudTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
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
};
texture skyLossTexture1;
sampler2D sky_loss_texture_1= sampler_state 
{
    Texture = <skyLossTexture1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
texture skyInscatterTexture1;
sampler2D sky_inscatter_texture_1= sampler_state 
{
    Texture = <skyInscatterTexture1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

texture skyLossTexture2;
sampler2D sky_loss_texture_2= sampler_state 
{
    Texture = <skyLossTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
texture skyInscatterTexture2;
sampler2D sky_inscatter_texture_2= sampler_state 
{
    Texture = <skyInscatterTexture2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};


float4 eyePosition : EYEPOSITION_WORLDSPACE;
// Light response: primary,secondary,anisotropic,ambient
float4 lightResponse;
float3 lightDir : Direction;
float3 skylightColour;
float3 sunlightColour;
float4 fractalScale;
float2 interp={0,1};

float4 lightningMultipliers;
float4 lightningColour;
float3 illuminationOrigin;
float3 illuminationScales;
float hazeEccentricity=0;
float3 mieRayleighRatio;
float fadeInterp=0;
float overcast=0;
float distance=1.0;
float3 cornerPos;
float3 texScales;
float layerFade;

struct vertexInput
{
    float3 position			: POSITION;
    float3 texCoords		: TEXCOORD0;
    float layerFade			: TEXCOORD1;
    float2 texCoordsNoise	: TEXCOORD2;
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
    float3 texCoords			: TEXCOORD2;
	float3 wPosition			: TEXCOORD3;
    float3 texCoordLightning	: TEXCOORD4;
    float2 fade_texc			: TEXCOORD5;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	
	OUT.texCoords=IN.texCoords;
	const float c=fractalScale.w;
	OUT.texCoordsNoise=IN.texCoordsNoise;
	OUT.wPosition=(IN.position.xyz-eyePosition.xyz);
	OUT.layerFade=IN.layerFade;
	// Note position.xzy is used!
	float3 texCoordLightning=(IN.position.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning=texCoordLightning;
	float3 view=normalize(OUT.wPosition.xyz);
	float sine=view.y;
	OUT.fade_texc=float2(length(OUT.wPosition.xyz)/200000.f,0.5f*(1.f-sine));
    return OUT;
}

#define pi (3.1415926536f)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	return (1.f-g2)/(4.f*pi*pow(1.f+g2-2.f*g*cos0,1.5f));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831f*(1.f+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1.f,1.f,1.f)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=(1.f-overcast)*BetaTotal*inscatter_factor.rgb;
	return colour;
}

float4 PS_WithLightning(vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	
	float3 loss1=tex2D(sky_loss_texture_1,IN.fade_texc).rgb;
	float3 loss2=tex2D(sky_loss_texture_2,IN.fade_texc).rgb;
    float3 loss=lerp(loss1,loss2,fadeInterp);
	float4 insc1=tex2D(sky_inscatter_texture_1,IN.fade_texc);
	float4 insc2=tex2D(sky_inscatter_texture_2,IN.fade_texc);
    float4 insc=lerp(insc1,insc2,fadeInterp);
	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=25.f*HenyeyGreenstein(0.87f,cos0);
	float3 inscatter=InscatterFunction(insc,cos0);
	cos0=saturate(cos0);
	float3 noiseval=(tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-.5f).xyz;
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=tex3D(cloud_density_1,pos);
	float4 density2=tex3D(cloud_density_2,pos);

	float4 lightning=tex3D(lightning_illumination,IN.texCoordLightning.xyz);

	density*=interp.y;
	density+=interp.x*density2;
	
	density.x=saturate(density.x+2.f*IN.layerFade-1.f);
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.x;
	float3 final=(density.z*Beta+lightResponse.w*density.y)*sunlightColour+ambient.rgb;
	//float l=dot(lightningMultipliers,lightning);
	//float3 lightningC=l*lightningColour.xyz;
	//float3 final=(density.z*Beta+lightResponse.w*density.y)*sunlightColour+ambient.rgb+lightningColour.w*lightningC;
	
	final*=loss.xyz;
	final+=inscatter.xyz;

	final*=opacity;

	//final+=lightningC*(opacity+IN.layerFade);
    return float4(final.rgb,opacity);
}

float4 PS_Clouds( vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	
	float3 loss1=tex2D(sky_loss_texture_1,IN.fade_texc).rgb;
	float3 loss2=tex2D(sky_loss_texture_2,IN.fade_texc).rgb;
    float3 loss=lerp(loss1,loss2,fadeInterp);
	float4 insc1=tex2D(sky_inscatter_texture_1,IN.fade_texc);
	float4 insc2=tex2D(sky_inscatter_texture_2,IN.fade_texc);
    float4 insc=lerp(insc1,insc2,fadeInterp);

	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=25.f*HenyeyGreenstein(0.87f,cos0);
	float3 inscatter=InscatterFunction(insc,cos0);
	cos0=saturate(cos0);
	float3 noiseval=(tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-.5f).xyz;
	noiseval*=0.25f+IN.texCoords.z;
	float3 pos=IN.texCoords.xyz+fractalScale.xyz*noiseval;
	float4 density=tex3D(cloud_density_1,pos);
	float4 density2=tex3D(cloud_density_2,pos);

	density*=interp.y;
	density+=interp.x*density2;

	density.x*=IN.layerFade;
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.x;
	float3 final=(density.z*Beta+lightResponse.w*density.y)*sunlightColour+ambient.rgb;
	
	final*=loss;
	final+=inscatter;
	final*=opacity;

    return float4(final.rgb,opacity);
}

vertexOutput VS_Main_New(vertexInputNew IN)
{
    vertexOutput OUT;
	float3 pos=distance*IN.position.xyz;
    OUT.hPosition=mul(worldViewProj,float4(pos,1.0));
    float3 wPosition=pos+eyePosition.xyz;
	const float c=fractalScale.w;
	OUT.texCoords=(wPosition.xzy-cornerPos)*texScales.xyz;
	OUT.texCoordsNoise=float2(0,0);
	OUT.wPosition=pos.xyz;
	OUT.layerFade=0;
	// Note position.xzy is used!
	float3 texCoordLightning=(IN.position.xzy-illuminationOrigin.xyz)/illuminationScales.xyz;
	OUT.texCoordLightning=texCoordLightning;
	float3 view=normalize(OUT.wPosition.xyz);
	float sine=view.y;
	OUT.fade_texc=float2(length(OUT.wPosition.xyz)/200000.f,0.5f*(1.f-sine));
    return OUT;
}

float4 PS_Clouds_New( vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	
	float3 loss1=tex2D(sky_loss_texture_1,IN.fade_texc).rgb;
	float3 loss2=tex2D(sky_loss_texture_2,IN.fade_texc).rgb;
    float3 loss=lerp(loss1,loss2,fadeInterp);
	float4 insc1=tex2D(sky_inscatter_texture_1,IN.fade_texc);
	float4 insc2=tex2D(sky_inscatter_texture_2,IN.fade_texc);
    float4 insc=lerp(insc1,insc2,fadeInterp);

	float cos0=dot(lightDir.xyz,view.xyz);
	float Beta=25.f*HenyeyGreenstein(0.87f,cos0);
	float3 inscatter=InscatterFunction(insc,cos0);
	cos0=saturate(cos0);
	float3 noiseval=(tex2D(noise_texture,IN.texCoordsNoise.xy).xyz-.5f).xyz;
	noiseval*=0.25f+IN.texCoords.z;
	float3 pos=IN.texCoords.xyz;//+fractalScale.xyz*noiseval;
	float4 density=tex3D(cloud_density_1,pos);
	float4 density2=tex3D(cloud_density_2,pos);

	density*=interp.y;
	density+=interp.x*density2;

	density.x*=layerFade;
	float3 ambient=density.w*skylightColour.rgb;

	float opacity=density.x;
	float3 final=(density.z*Beta+lightResponse.w*density.y)*sunlightColour+ambient.rgb;
	
	final*=loss;
	final+=inscatter;
	final*=opacity;
    return float4(final.rgb,opacity);
}

technique simul_clouds
{
    pass p0 
    {
		zenable = false;
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

technique simul_clouds_new_method
{
    pass p0 
    {
		zenable = false;
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

		VertexShader = compile vs_2_0 VS_Main_New();
		PixelShader  = compile ps_2_0 PS_Clouds_New();
    }
}

technique simul_clouds_and_lightning
{
    pass p0 
    {
		zenable = false;
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
		PixelShader  = compile ps_2_0 PS_WithLightning();
    }
}

