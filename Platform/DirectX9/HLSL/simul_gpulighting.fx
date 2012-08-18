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
float octaves;
float persistence;
float humidity;
float time;
const float zPixel=1.0/8.0;
int zSize;
const float pi=3.1415926536;

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

float NoiseFunction(float3 pos,int oct,float pers)
{
	float dens=0;
	float mul=0.5f;
	int octave_3d=1;
	float this_grid_height=8.0;
	for(int i=0;i<5;i++)
	{
		if(i>=oct)
			return dens;
		int kmax=max(1,(int)(this_grid_height+0.5));
		float lookup=tex3D(volume_noise_texture,pos).x;
		float val=0.5+0.5*sin(2.0*pi*(lookup+time));
		dens+=mul*lookup;//val;
		mul*=pers;
		pos*=2.f;
		this_grid_height*=2;
	}
	return dens;
}


	float GetHumidityMultiplier(float z)
	{
		const float base_layer=0.46f;
		const float transition=0.2f;
		const float mul=0.7f;
		const float default_=1.f;
		float i=saturate((z-base_layer)/transition);
		return default_*(1.f-i)+mul*i;
	}

float GetDensity(float3 densityspace_texcoord)
{
	float3 noisespace_texcoord=float3(densityspace_texcoord.xy,densityspace_texcoord.z/(float)zSize);
	float noise_val=NoiseFunction(noisespace_texcoord,octaves,persistence);
	float mul=GetHumidityMultiplier(densityspace_texcoord.z);
	mul=saturate(densityspace_texcoord.z/zPixel)*saturate((1.0-densityspace_texcoord.z)/zPixel);

float diffusivity=0.02;
	float dens=saturate((noise_val+humidity*mul-1.0)/diffusivity);
	return 1.0;
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
    return float4(direct_light,indirect_light,0,0);
}

float4 GPUTransformPS(v2f IN): COLOR
{
	float2 texcoord=IN.texcoord.xy;
	texcoord+=texCoordOffset;
	texcoord.y*=zSize;
	float zPos=(floor(texcoord.y)+0.5)/(float)zSize;
	texcoord.y=frac(texcoord.y);
	float3 densityspace_texcoord=float3(texcoord.xy,zPos);
	float3 ambient_texcoord=float3(texcoord.xy,1.0-zPixel/2.0-zPos);
	float3 noisespace_texcoord=float3(densityspace_texcoord.xy,densityspace_texcoord.z/8.f);

	float3 lightspace_texcoord=mul(transformMatrix,float4(densityspace_texcoord,1.0)).xyz;
	float2 light_lookup=tex3D(direct_light_texture,lightspace_texcoord).xy;
	float2 amb_texel=tex3D(ambient_light_texture,ambient_texcoord).xy;
	float ambient_lookup=0.5*(amb_texel.x+amb_texel.y);
	float density=GetDensity(densityspace_texcoord);
	//float density=tex3D(cloud_density,densityspace_texcoord).x;
	float4 result=float4(density,light_lookup.x,light_lookup.y,ambient_lookup);
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
		ColorWriteEnable=RED|GREEN; 
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