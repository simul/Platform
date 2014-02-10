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
	OUT.wPosition=IN.position.xyz;
    return OUT;
}

#define pi (3.1415926536f)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.f+g2-2.f*g*cos0;
	return (1.f-g2)/(4.f*pi*sqrt(u*u*u));
}

float3 InscatterFunction(float4 inscatter_factor,float cos0)
{
	float BetaRayleigh=CalcRayleighBeta(cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1.f,1.f,1.f)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

#define SMOOTH_BLENDING
float4 PS_Main( vertexOutput IN): color
{
	float3 view=normalize(IN.wPosition.xyz-eyePosition.xyz);
	float cos0=dot(lightDir.xyz,view.xyz);

	float3 noiseval=(tex2D(noiseSampler,IN.texCoordNoise.xy).xyz).xyz-.5f;
#if 1//def DETAIL_NOISE
	noiseval+=(tex2D(noiseSampler,IN.texCoordNoise.xy*8).xyz-.5f)/2.0;
#endif
	float2 pos=0.01*IN.texCoords+fractalScale.xy*(noiseval.xy);
	float4 density=tex2D(cloud_density_1,pos.xy);
	density.x=tex2D(cloud_density_1,pos.xy).x;
	float4 density2=tex2D(cloud_density_2,pos.xy);
	density2.x=tex2D(cloud_density_2,pos.xy).x;
	float Beta=HenyeyGreenstein(cloudEccentricity,cos0);

	float3 image=tex2D(imageSampler,IN.imageCoords.xy);
	density=lerp(density,density2,interp.x);


//	density.x*=(.5f+imageEffect*(image.x-0.5f));
#ifdef SMOOTH_BLENDING
//	density.x*=3.f;
//	density.x-=1.f;
//	density.x=saturate(density.x);
#endif

	float opacity=saturate(density.x);

	//density.z=1.f-((1.f-density.x)*(1.f-density.z));

	float3 final=(lightResponse.x*Beta*density.z+lightResponse.y*density.y)*sunlightColour+ambientColour.rgb;
	//float3 final=(lightResponse.x*Beta*density.z+lightResponse.y*density.y)*sunlightColour+ambientColour.rgb;

	final*=IN.loss;
	final+=IN.inscatter;
    return float4(final,opacity);
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
		SrcBlend = SrcAlpha;
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

