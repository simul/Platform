#include "dx9.hlsl"
#include "../../CrossPlatform/SL/atmospherics_constants.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/atmospherics.sl"

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
sampler2D loss_texture= sampler_state 
{
    Texture = <lossTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

texture inscTexture;
sampler2D insc_texture= sampler_state 
{
    Texture = <inscTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

texture skylTexture;
sampler2D skyl_texture =sampler_state 
{
    Texture = <skylTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};

texture illuminationTexture;
sampler2D illumination_texture =sampler_state 
{
    Texture = <illuminationTexture>;
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

// For Godrays:
float3 cloudScales;
float3 cloudOffset;
float3 lightColour;
float3 eyePosition;
float cloudInterp;

// For Lightning Airglow:
float4 lightningMultipliers;
float4 lightningColour;
float3 illuminationOrigin;
float3 illuminationScales;

#ifndef pi
#define pi (3.1415926536f)
#endif

struct atmosVertexOutput
{
    vec4 hPosition		: POSITION;
    vec2 clip_pos		: TEXCOORD0;
    vec2 texCoords		: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(vertexInputPositionOnly IN)
{
	atmosVertexOutput OUT;
	//OUT.position		=vec4(IN.position.xy,0,1);
	OUT.clip_pos.xy		=IN.position.xy;
	//OUT.texCoords		=0.5*vec2(IN.position.x+1.0,1.0-IN.position.y);
	OUT.hPosition	=vec4(IN.position.xy,0.f,1.f);
	OUT.texCoords	=0.5*(IN.position.xy+vec2(1.0,1.0));
	OUT.texCoords.y	=1.0-OUT.texCoords.y;
	return OUT;
}

vec4 PS_AtmosOverlayLossPass(atmosVertexOutput IN) : color
{
	vec3 loss=AtmosphericsLoss(depth_texture
							,viewportToTexRegionScaleBias
							,loss_texture
							,invViewProj
							,IN.texCoords
							,IN.clip_pos
							,depthToLinFadeDistParams
							,tanHalfFov);
    return float4(loss.rgb,1.f);
}

vec4 PS_AtmosOverlayInscPass(atmosVertexOutput IN) : color
{
	vec3 insc=Inscatter(	insc_texture
							,skyl_texture
							,depth_texture
							,1
							,illumination_texture
							,invViewProj
							,IN.texCoords
							,lightDir
							,hazeEccentricity
							,mieRayleighRatio
							,viewportToTexRegionScaleBias
							,depthToLinFadeDistParams
							,tanHalfFov
							,false,false);

	return vec4(insc*exposure,1.0);
}

vec4 PS_Godrays(atmosVertexOutput IN) : color
{
	return vec4(0.0,0.0,0.0,1.0);
}

// height of target above viewer is equal to depth*sin(elevation).
// so height of viewer above target is dh=(-depth*sine)
// but fog layer is present, and viewer has heightAboveFogLayer.
// if target is between fog layer and viewer then dh<heightAboveFogLayer;
// Fog is exponential. Optical density is equal to f=exp(-fog*dh).
// then colour->colour f+fogColour(1-f)
float heightAboveFogLayer=0.f;// in depth units.
float3 fogExtinction;

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
    pass loss_pass
    {
		cullmode = none;
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_AtmosOverlayLossPass();
		AlphaBlendEnable = true;
		ZWriteEnable = false;
		ZEnable= false;
		SrcBlend = Zero;
		DestBlend = SrcColor;
    }
    pass insc_pass
    {
		cullmode = none;
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_AtmosOverlayInscPass();
		AlphaBlendEnable = true;
		ZWriteEnable = false;
		ZEnable= false;
		SrcBlend = One;
		DestBlend = One;
    }
}

technique simul_godrays
{
    pass
    {
		cullmode = none;
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
		cullmode = none;
		VertexShader = compile vs_3_0 VS_Atmos();
		PixelShader = compile ps_3_0 PS_Airglow();
		alphablendenable = true;
		ZWriteEnable = true;
		ZEnable= false;
		SrcBlend = One;
		DestBlend = One;
    }
}
