float4x4 invViewProj;

texture maxDistanceTexture;
sampler2D distance_texture= sampler_state 
{
    Texture = <maxDistanceTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Mirror;
	AddressV = Clamp;
};

texture depthTexture;
sampler2D depth_texture= sampler_state 
{
    Texture = <depthTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

texture imageTexture;
sampler2D image_texture= sampler_state 
{
    Texture = <imageTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

texture lossTexture;
sampler2D sky_loss_texture_1= sampler_state 
{
    Texture = <lossTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
texture inscTexture;
sampler2D sky_inscatter_texture_1= sampler_state 
{
    Texture = <inscTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

texture skylTexture;
sampler2D skylight_texture sampler_state 
{
    Texture = <skylTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
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

// For distance-fade:
float4 lightDir;
float4 mieRayleighRatio;
float hazeEccentricity;
float fadeInterp;
float altitudeTexCoord;

// For Godrays:
float3 cloudScales;
float3 cloudOffset;
float3 lightColour;
float3 eyePosition;
float2 texelOffsets;
float cloudInterp;

// For Lightning Airglow:
float4 lightningMultipliers;
float4 lightningColour;
float3 illuminationOrigin;
float3 illuminationScales;

#define pi (3.1415926536f)

struct atmosVertexInput
{
    float2 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct atmosVertexOutput
{
    float4 position			: POSITION;
    float4 hpos_duplicate	: TEXCOORD0;
    float2 texCoords		: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(atmosVertexInput IN)
{
	atmosVertexOutput OUT;
	OUT.position=float4(IN.position.xy,0,1);
	OUT.hpos_duplicate=float4(IN.position.xy,0,1);
	OUT.texCoords=IN.texCoords;
	//OUT.texCoords*=(float2(1.0,1.0)+texelOffsets);
	OUT.texCoords+=0.5*texelOffsets;
	return OUT;
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return (1.f-g2)/(4.f*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831f*(1.f+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}
// height of target above viewer is equal to depth*sin(elevation).
// so height of viewer above target is dh=(-depth*sine)
// but fog layer is present, and viewer has heightAboveFogLayer.
// if target is between fog layer and viewer then dh<heightAboveFogLayer;
// Fog is exponential. Optical density is equal to f=exp(-fog*dh).
// then colour->colour f+fogColour(1-f)
float heightAboveFogLayer=0.f;// in depth units.
float4 fogColour;

float3 fogExtinction;
float fogDensity;



float4 PS_Atmos(atmosVertexOutput IN) : color
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x+texelOffsets.x;
	pos.y-=2.f*IN.texCoords.y+texelOffsets.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float4 lookup=tex2D(image_texture,IN.texCoords.xy);
	float3 colour=lookup.rgb;
	float depth=lookup.a;
	if(depth>=1.f)
		discard;
#ifdef Y_VERTICAL
	float sine=view.y;
#else
	float sine=view.z;
#endif
	float dh=-(depth*sine);
	
	float3 f=max(1.f-fogDensity,exp(min(0,-(dh-heightAboveFogLayer)*fogExtinction)));
	float maxd=1.0;//tex2D(distance_texture,texc2).x;
	float2 texc2=float2(pow(depth/maxd,0.5f),0.5f*(1.f-sine));
	float3 loss=tex2D(sky_loss_texture_1,texc2).rgb;
	colour*=f;
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(fogColour,cos0)*(1-f);
	colour*=loss;
	float4 inscatter_factor=tex2D(sky_inscatter_texture_1,texc2);
	colour+=InscatterFunction(inscatter_factor,cos0);
    return float4(colour,1.f);
}

float4 PS_Godrays(atmosVertexOutput IN) : color
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y-=2.f*IN.texCoords.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float4 lookup=tex2D(image_texture,IN.texCoords.xy);
	float depth=lookup.a;
	float light=0;
#ifdef Y_VERTICAL
	if(lightDir.y<0.1)
#else
	if(lightDir.z<0.1)
#endif
		discard;
#ifdef Y_VERTICAL
	float3 lightVec=lightDir/lightDir.y;
#else
	float3 lightVec=lightDir/lightDir.z;
#endif
	float filter=1.f;
	for(int i=0;i<64;i++)
	{
		float3 wPosition=view;
		float d=(64-i-0.5)/64.0;
		float thickness=2.0*d+1.0;
		d*=d;
		float dd=depth*10.f-d;
		thickness*=saturate(10.f*dd/thickness);
		wPosition*=d*30000.0;
		wPosition+=eyePosition.xyz;
#ifdef Y_VERTICAL
		float dz=cloudOffset.z-wPosition.y;
#else
		float dz=cloudOffset.z-wPosition.z;
#endif
		thickness*=saturate(dz/1000.0);
#if 0
		float3 cloud_texc=(wPosition.xzy-cloudOffset.xyz)*cloudScales.xyz;
		cloud_texc.z=0.5;
		float4 cloud1=tex3D(cloud_texture1,cloud_texc);
		float4 cloud2=tex3D(cloud_texture2,cloud_texc);
		float4 cloud=lerp(cloud1,cloud2,cloudInterp);
		filter*=1.f-0.05*thickness*cloud.x;
#endif
		wPosition.xyz+=lightVec*dz;
		float3 cloud_texc=(wPosition.xzy-cloudOffset.xyz)*cloudScales.xyz;
		float4 cloud1=tex3D(cloud_texture1,cloud_texc);
		float4 cloud2=tex3D(cloud_texture2,cloud_texc);
		float4 cloud=lerp(cloud1,cloud2,cloudInterp);
		//light*=(1.f-cloud.x);
		light+=0.002*thickness*cloud.z;
	}
    return float4(light*lightColour,filter);
}

float4 PS_Airglow(atmosVertexOutput IN) : color
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x;
	pos.y-=2.f*IN.texCoords.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float4 lookup=tex2D(image_texture,IN.texCoords.xy);
	float depth=lookup.a;
	float4 final=float4(0,0,0,0);
	float3 lightVec=lightDir/lightDir.y;
	for(int i=0;i<64;i++)
	{
		float3 wPosition=view;
		float d=(64-i-0.5)/64.0;
		float thickness=d/32.0;
		d*=d;
		float dd=depth*10.f-d;
		thickness*=saturate(10.f*dd/thickness);
		wPosition*=d*30000.0;
		wPosition+=eyePosition.xyz;
		float dz=cloudOffset.z-wPosition.y;
		//thickness*=saturate(dz/1000.0);
		float3 lightning_texc=(wPosition.xzy-illuminationOrigin.xyz)*illuminationScales.xyz;
		float4 lightning=tex3D(lightning_illumination,lightning_texc.xyz);
		//lightning.xyzw=lightning.xyzw*lightningMultipliers.xyzw;
//lightning.rgb+=lightning.a;
		float l=dot(lightningMultipliers.xyzw,lightning.xyzw);
		float4 lightningC=l*lightningColour.xyzw;
		
		final+=12.f*thickness*lightningColour.w*lightningC;
	}
	final.a=1;
    return final;
}

technique simul_atmospherics
{
    pass
    {
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_Atmos();
		alphablendenable = false;
		ZWriteEnable = true;
		ZEnable= false;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
    }
}

technique simul_godrays
{
    pass
    {
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_Godrays();
		alphablendenable = true;
		ZWriteEnable = true;
		ZEnable= false;
		SrcBlend = One;
		DestBlend = SrcAlpha;
    }
}

technique simul_airglow
{
    pass
    {
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_Airglow();
		alphablendenable = true;
		ZWriteEnable = true;
		ZEnable= false;
		SrcBlend = One;
		DestBlend = One;
    }
}
