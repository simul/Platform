float4x4 lightToDensityMatrix;

Texture3D cloudDensityTexture;
SamplerState samplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
	AddressW = Clamp;
};

float zPosition;
float2 extinctions;
float2 texCoordOffset;

Texture2D inputLightTexture;

struct a2v
{
    float4 position:POSITION;
    float4 texcoord:TEXCOORD0;
};

struct v2f
{
    float4 position:SV_POSITION;
    float4 texcoord:TEXCOORD0;
};

v2f GPULightVS(a2v IN)
{
	v2f OUT;
	OUT.position=IN.position;
	OUT.texcoord=IN.texcoord;
    return OUT;
}

float4 GPULightPS(v2f IN) : SV_TARGET
{
	float2 texcoord=IN.texcoord.xy+texCoordOffset;
	float4 previous_light=inputLightTexture.Sample(samplerState,texcoord.xy);
	float3 lightspace_texcoord=float3(texcoord.xy,zPosition);
	float3 densityspace_texcoord=mul(lightToDensityMatrix,float4(lightspace_texcoord,1.0)).xyz;
	float4 dens_lookup=cloudDensityTexture.Sample(samplerState,densityspace_texcoord);
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

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

technique11 simul_gpulighting
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,GPULightVS()));
		SetPixelShader(CompileShader(ps_4_0,GPULightPS()));
    }
}
