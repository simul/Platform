#include "AtmosphericsUniforms.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/depth.sl"

Texture2D depthTexture;
Texture2D imageTexture;
Texture2D lossTexture;
Texture2D inscatterTexture;
Texture2D skylightTexture;

#define pi (3.1415926536)

struct atmosVertexInput
{
    float2 position			: POSITION;
    float2 texCoords		: TEXCOORD0;
};

struct atmosVertexOutput
{
    float4 position			: SV_POSITION;
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

float4 PS_Atmos(atmosVertexOutput IN) : SV_TARGET
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x+texelOffsets.x;
	pos.y-=2.f*IN.texCoords.y+texelOffsets.y;
	float3 view=normalize(mul(invViewProj,pos).xyz);
	float4 lookup=imageTexture.Sample(clampSamplerState,IN.texCoords.xy);
	float4 dlookup=depthTexture.Sample(clampSamplerState,IN.texCoords.xy);
	
	float3 colour=lookup.rgb;
	float depth=dlookup.r;
	float dist=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
#ifdef REVERSE_DEPTH
	if(depth<=0.0)
		dist=1.0;
#else
	if(depth>=1.0)
		dist=1.0;
#endif
	float sine=view.z;
	float2 texc2=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float3 loss=lossTexture.Sample(clampSamplerState,texc2).rgb;
	colour*=loss;
	float4 inscatter_factor=inscatterTexture.Sample(clampSamplerState,texc2);
	float cos0=dot(view.xyz,lightDir.xyz);
	colour+=InscatterFunction(inscatter_factor,cos0);
	colour+=skylightTexture.Sample(clampSamplerState,texc2);

    return float4(colour.rgb,1.f);
}

float4 PS_AtmosOverlayLossPass(atmosVertexOutput IN) : SV_TARGET
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x+texelOffsets.x;
	pos.y-=2.f*IN.texCoords.y+texelOffsets.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float depth=depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	if(depth>=1.f)
		discard;
	float sine=view.z;
	float2 texc2=float2(pow(depth,0.5f),0.5f*(1.f-sine));
	float3 loss=lossTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(loss,1.f);
}

float4 PS_AtmosOverlayInscPass(atmosVertexOutput IN) : SV_TARGET
{
	float4 pos=float4(-1.f,1.f,1.f,1.f);
	pos.x+=2.f*IN.texCoords.x+texelOffsets.x;
	pos.y-=2.f*IN.texCoords.y+texelOffsets.y;
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float depth=depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	if(depth>=1.f)
		discard;
	float sine=view.z;
	float2 texc2=float2(pow(depth,0.5f),0.5f*(1.f-sine));
	float4 inscatter_factor=inscatterTexture.Sample(clampSamplerState,texc2);
	float cos0=dot(view,lightDir);
	float3 colour=InscatterFunction(inscatter_factor,cos0);
	colour+=skylightTexture.Sample(clampSamplerState,texc2);
    return float4(colour,1.f);
}

technique11 simul_atmospherics
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_Atmos()));
    }
}

technique11 simul_atmospherics_depthtexture
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayLossPass()));
    }
    pass p1
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Atmos()));
		SetPixelShader(CompileShader(ps_4_0,PS_AtmosOverlayInscPass()));
    }
}

