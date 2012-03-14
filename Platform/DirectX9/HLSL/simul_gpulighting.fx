float4x4 transformMatrix;// either lightToDensityMatrix or vice versa;

texture cloudDensityTexture;
	
sampler3D cloud_density= sampler_state 
{
    Texture = <cloudDensityTexture>;
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

texture volumeNoiseTexture;
sampler3D volume_noise_texture= sampler_state 
{
    Texture = <volumeNoiseTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
	AddressW = Wrap;
	SRGBTexture = 0;
};
texture inputDirectLightTexture;
sampler3D direct_light_texture= sampler_state 
{
    Texture = <inputDirectLightTexture>;
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
texture inputIndirectLightTexture;
sampler3D indirect_light_texture= sampler_state 
{
    Texture = <inputIndirectLightTexture>;
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
texture inputAmbientLightTexture;
sampler3D ambient_light_texture= sampler_state 
{
    Texture = <inputAmbientLightTexture>;
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

float zPosition;
float2 extinctions;
float2 texCoordOffset;

texture inputLightTexture;
sampler2D input_light_texture= sampler_state 
{
    Texture = <inputLightTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
	SRGBTexture = 0;
};

struct a2v
{
    float4 position:POSITION;
    float4 texcoord:TEXCOORD0;
};

struct v2f
{
    float4 position:POSITION;
    float4 texcoord:TEXCOORD0;
};

v2f GPULightVS(a2v IN)
{
	v2f OUT;
	OUT.position=IN.position;
	OUT.texcoord=IN.texcoord;
    return OUT;
}

float NoiseFunction(float3 pos,int octaves,float persistence)
{
	float dens=0;
	float mul=0.5f;
	for(int i=0;i<octaves;i++)
	{
		dens+=mul*tex3D(volume_noise_texture,pos).x;
		mul*=persistence;
		pos*=2.f;
	}
	return dens;
}

float GetDensity(float3 densityspace_texcoord)
{
	float3 noisespace_texcoord=float3(densityspace_texcoord.xy,densityspace_texcoord.z/8.f);
	float noise_val=NoiseFunction(noisespace_texcoord,3,0.65f);
	const float pixel=1.0/8.0;
	float mul=saturate(densityspace_texcoord.z/pixel)*saturate((1.0-densityspace_texcoord.z)/pixel);
float active_humidity=0.5;
float diffusivity=0.005;
	float dens=saturate((noise_val+active_humidity*mul-1.0)/diffusivity);
	return dens;
}

float4 GPULightPS(v2f IN) : COLOR
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float4 previous_light=tex2D(input_light_texture,texcoord.xy);
	float3 lightspace_texcoord=float3(texcoord.xy,zPosition);
	float3 densityspace_texcoord=mul(transformMatrix,float4(lightspace_texcoord,1.0)).xyz;

	//float density=tex3D(cloud_density,densityspace_texcoord).x;
	float density=GetDensity(densityspace_texcoord);
	
	float previous_density=previous_light.z;

	float direct_light=previous_light.x*exp(-extinctions.x*density);
	float indirect_light=previous_light.y*exp(-extinctions.y*density);
	indirect_light=saturate(indirect_light);
    return float4(direct_light,indirect_light,density,0.25f);
}

float4 GPUTransformPS(v2f IN): COLOR
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float3 densityspace_texcoord=float3(texcoord.xy,zPosition);
	float3 noisespace_texcoord=float3(densityspace_texcoord.xy,densityspace_texcoord.z/8.f);

	float3 lightspace_texcoord=mul(transformMatrix,float4(densityspace_texcoord,1.0)).xyz;
	float light_lookup=tex3D(direct_light_texture,lightspace_texcoord).x;
	float indirect_lookup=tex3D(indirect_light_texture,lightspace_texcoord).x;
	float ambient_lookup=tex3D(ambient_light_texture,densityspace_texcoord).x;
	float density=GetDensity(densityspace_texcoord);
	//float density=tex3D(cloud_density,densityspace_texcoord).x;
	float4 result=float4(density,indirect_lookup,light_lookup,ambient_lookup);
	return result;
}

technique simul_gpulighting
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		VertexShader = compile vs_3_0 GPULightVS();
		PixelShader = compile ps_3_0 GPULightPS();
    }
}

technique simul_transform_lightgrid
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		AlphaTestEnable=false;
		VertexShader = compile vs_3_0 GPULightVS();
		PixelShader = compile ps_3_0 GPUTransformPS();
    }
}