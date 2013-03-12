float4x4 transformMatrix;// either lightToDensityMatrix or vice versa;

texture cloudDensityTexture;
	
sampler3D cloud_density_texture= sampler_state 
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

texture inputAmbientLightTexture;
sampler3D ambient_light_texture= sampler_state 
{
    Texture = <inputAmbientLightTexture>;
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
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
int octaves;
float persistence;
float humidity;
float time;
float zPixel;
int zSize;
float3 noiseScale;
#define pi (3.1415926536)

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

float3 assemble3dTexcoord(float2 texcoord2)
{
	float2 texcoord	=texcoord2.xy;
	texcoord.y		*=zSize;
	float zPos		=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y		=frac(texcoord.y);
	float3 texcoord3	=float3(texcoord.xy,zPos);
	return texcoord3;
}

float NoiseFunction(float3 pos)
{
	float dens=0;
	float mul=0.5f;
	//int octave_3d=1;
	float this_grid_height=1.0;
	float sum=0;
	float t=time;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		int kmax=max(1,(int)(this_grid_height+0.5));
		float lookup=tex3D(volume_noise_texture,pos).x;
		float val=lookup;//0.5+0.5*sin(2.0*pi*(lookup+t));//
		//val*=saturate(pos.z*8.f+.5f);
		//val*=saturate(kmax+0.5f-pos.z*8.f);
		dens+=mul*val;
		sum+=mul;
		mul*=persistence;
		pos*=2.f;
		this_grid_height*=2;
		t*=2.f;
	}
	dens/=sum;
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
	float3 noisespace_texcoord=densityspace_texcoord*noiseScale+float3(1.0,1.0,0);
	//float3(densityspace_texcoord.xy,densityspace_texcoord.z/(float)zSize);
	float noise_val=NoiseFunction(noisespace_texcoord);
	float hm=humidity*GetHumidityMultiplier(densityspace_texcoord.z);

float diffusivity=0.02;
	float dens=saturate((noise_val+hm-1.0)/diffusivity);
//dens*=mul;
	dens*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
	//
		//saturate((densityspace_texcoord.z-0.5*zPixel)/zPixel)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)*.5/zPixel);

	return dens;
}

float4 GPUDensityPS(v2f IN) : COLOR
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float3 densityspace_texcoord=assemble3dTexcoord(texcoord.xy);
	float density=GetDensity(densityspace_texcoord);
    return float4(density,density,density,density);
}

float4 GPULightPS(v2f IN) : COLOR
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float4 previous_light=tex2D(input_light_texture,texcoord.xy);
	float3 lightspace_texcoord=float3(texcoord.xy,zPosition);
	float3 densityspace_texcoord=mul(transformMatrix,float4(lightspace_texcoord,1.0)).xyz;
//densityspace_texcoord.z+=zPixel/2.f;
	float density=tex3D(cloud_density_texture,densityspace_texcoord).x;
	float direct_light=previous_light.x*exp(-extinctions.x*density);
	float indirect_light=previous_light.y*exp(-extinctions.y*density);
	indirect_light=saturate(indirect_light);
    return float4(direct_light,indirect_light,0,0);
}


float4 GPUTransformPS(v2f IN): COLOR
{
	float3 densityspace_texcoord=assemble3dTexcoord(IN.texcoord.xy);
	float3 ambient_texcoord=float3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);

	float3 lightspace_texcoord=mul(transformMatrix,float4(densityspace_texcoord,1.0)).xyz;
	float2 light_lookup=tex3D(direct_light_texture,lightspace_texcoord).xy;
	float2 amb_texel=tex3D(ambient_light_texture,ambient_texcoord).xy;
	float ambient_lookup=0.5*(amb_texel.x+amb_texel.y);
	float density=tex3D(cloud_density_texture,densityspace_texcoord).x;
	float4 result=float4(density,light_lookup.xy,ambient_lookup);
	return result;
}


technique cloud_density
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
		PixelShader = compile ps_3_0 GPUDensityPS();
    }
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