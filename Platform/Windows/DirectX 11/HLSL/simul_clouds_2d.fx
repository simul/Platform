float4x4 worldViewProj	: WorldViewProjection;

texture cloudDensity1;
sampler2D cloud_density_1= sampler_state 
{
    Texture = <cloudDensity1>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture cloudDensity2;
sampler2D cloud_density_2= sampler_state 
{
    Texture = <cloudDensity2>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture noiseTexture;
sampler2D noiseSampler= sampler_state 
{
    Texture = <noiseTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture imageTexture;
sampler2D imageSampler= sampler_state 
{
    Texture = <imageTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

float3 sunlightColour=float3(1,1,1);
float4 eyePosition : EYEPOSITION_WORLDSPACE = {0,0,0,0};

float4 lightResponse;
float3 ambientColour ={0,0,0};
float4 lightDir : Direction = {1.0f, -1.0f, 1.0f, 0.0f};
float4 fractalScale={600.f/80000.f,600.f/80000.f,600.f/3500.f,0};
const float pi=3.1415926536f;
float2 interp={0,1};
float layerDensity=1.f;
float imageEffect=.2f;
float hazeEccentricity=0;
float3 mieRayleighRatio;
float cloudEccentricity=0.87f;

struct vertexInput
{
    float3 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
    float4 loss				: TEXCOORD1;
    float4 inscatter		: TEXCOORD2;
    float2 texCoordNoise	: TEXCOORD3;
	float2 imageCoords		: TEXCOORD4;
};

struct vertexOutput
{
    float4 hPosition		: POSITION;
    float2 texCoords		: TEXCOORD1;
    float4 loss				: TEXCOORD2;
    float4 inscatter		: TEXCOORD3;
    float2 texCoordNoise	: TEXCOORD4;
	float2 imageCoords		: TEXCOORD5;
	float3 wPosition		: TEXCOORD6;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texCoords=IN.texCoords;
	OUT.texCoordNoise=IN.texCoordNoise;
	OUT.imageCoords=IN.imageCoords;
	OUT.loss=IN.loss;
	OUT.inscatter=IN.inscatter;
	OUT.wPosition=normalize(IN.position.xyz-eyePosition.xyz);
    return OUT;
}

#define pi (3.1415926536f)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	return 0.5*0.079577+0.5*(1.f-g2)/(4.f*pi*pow(1.f+g2-2.f*g*cos0,1.5f));
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

//#define SMOOTH_BLENDING
float4 PS_Main( vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition);
	float cos0=dot(lightDir.xyz,view.xyz);
	//cos0=pow(cos0,36.f);
	float3 noiseval=(tex2D(noiseSampler,IN.texCoordNoise.xy).xyz).xyz;
	float2 pos=IN.texCoords+fractalScale.xy*(noiseval.xy-.5f);
	float4 density=tex2D(cloud_density_1,pos.xy);
	float4 density2=tex2D(cloud_density_2,pos.xy);
	float Beta=HenyeyGreenstein(cloudEccentricity,cos0);

	float3 image=tex2D(imageSampler,IN.imageCoords.xy);
	density=lerp(density,density2,interp.x);

#ifdef SMOOTH_BLENDING
	density.x*=3.f;
	density.x-=1.f;
	density.x=saturate(density.x);
#endif
	density.x*=(.5f+imageEffect*(image.x-0.5f));

	float opacity=saturate(density.x);
	float3 final=(lightResponse.x*Beta*density.z+lightResponse.y*density.y)*sunlightColour+density.w*ambientColour.rgb;

	final*=IN.loss;
	final+=IN.inscatter;
	final.rgb*=opacity;
    return float4(final.rgb,opacity);
}

technique simul_clouds_2d
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

		VertexShader = compile vs_3_0 VS_Main();
		PixelShader  = compile ps_3_0 PS_Main();
    }
}

