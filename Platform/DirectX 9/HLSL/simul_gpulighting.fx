float4x4 lightToDensityMatrix;

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

float4 GPULightPS(v2f IN) : COLOR
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float4 previous_light=tex2D(input_light_texture,texcoord.xy);
	float3 lightspace_texcoord=float3(texcoord.xy,zPosition);
	float3 densityspace_texcoord=mul(lightToDensityMatrix,float4(lightspace_texcoord,1.0)).xyz;
	float4 dens_lookup=tex3D(cloud_density,densityspace_texcoord);
/*
	float ff=2.f;
	previous_light+=tex2D(input_light_texture,texcoord+float2(ff*texCoordOffset.x,0));
	previous_light+=tex2D(input_light_texture,texcoord-float2(ff*texCoordOffset.x,0));
	previous_light+=tex2D(input_light_texture,texcoord+float2(0,ff*texCoordOffset.y));
	previous_light+=tex2D(input_light_texture,texcoord-float2(0,ff*texCoordOffset.y));
	previous_light/=5.f;*/

	float previous_density=previous_light.z;

	float direct_light=previous_light.x*exp(-extinctions.x*dens_lookup.x);
	float indirect_light=previous_light.y*exp(-extinctions.y*dens_lookup.x);//+saturate(previous_light.x*previous_density/4.f);
	indirect_light=saturate(indirect_light);
    return float4(direct_light,indirect_light,dens_lookup.x,0.25f);
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
