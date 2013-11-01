float4x4 worldViewProj	: WorldViewProjection;
float4x4 world			: World;
float4 eyePosition		: EYEPOSITION_WORLDSPACE;
float4 mieRayleighRatio;
float4 lightDir;
float hazeEccentricity=0;
float fadeInterp=0;
#define pi (3.1415926536f)

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
struct vertexInput
{
    float3 position		: POSITION;
};

struct vertexOutput
{
    float4 hPosition	: POSITION;
    float3 wPosition	: TEXCOORD1;
};

vertexOutput VS_Main(vertexInput IN)
{
    vertexOutput OUT;
    OUT.hPosition=mul(worldViewProj, float4(IN.position.xyz , 1.0));
    OUT.wPosition = mul(world, float4(IN.position.xyz,1.f));
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
	float BetaRayleigh=CalcRayleighBeta(cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

half4 PS_Main(vertexOutput IN): color
{
	float3 difference=IN.wPosition-eyePosition;
	float distance=length(difference);
	float3 view=normalize(difference);
	float sine=view.y;
	float2 texc=float2(distance/200000.f,0.5f-0.5f*sine);
	float3 loss1=tex2D(sky_loss_texture_1,texc).rgb;
	float3 loss2=tex2D(sky_loss_texture_2,texc).rgb;
    float3 loss=lerp(loss1,loss2,fadeInterp);
	float4 insc1=tex2D(sky_inscatter_texture_1,texc);
	float4 insc2=tex2D(sky_inscatter_texture_2,texc);
    float4 insc=lerp(insc1,insc2,fadeInterp);
	float cos0=dot(view,lightDir);
	float3 inscatter=InscatterFunction(insc,cos0);
	float3 final;
	final=.17f*loss+inscatter;
    return half4(final,1.h);
}

technique simul_object
{
    pass p0 
    {
		VertexShader = compile vs_2_0 VS_Main();
		PixelShader  = compile ps_2_0 PS_Main();
    }
}

