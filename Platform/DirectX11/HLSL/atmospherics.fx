#include "CppHlsl.hlsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
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
	uint vertex_id			: SV_VertexID;
};

struct atmosVertexOutput
{
    float4 position			: SV_POSITION;
    float2 texCoords		: TEXCOORD0;
    float2 pos				: TEXCOORD1;
};

atmosVertexOutput VS_Atmos(atmosVertexInput IN)
{
	atmosVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.pos			=poss[IN.vertex_id];
	OUT.position	=float4(OUT.pos,0.0,1.0);
	// Set to far plane so can use depth test as want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(float2(1.0,1.0)+vec2(OUT.pos.x,-OUT.pos.y));
	OUT.texCoords	+=0.5*texelOffsets;
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
	float3 view=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	float4 lookup=imageTexture.Sample(clampSamplerState,IN.texCoords.xy);
	float4 dlookup=depthTexture.Sample(clampSamplerState,IN.texCoords.xy);
	
	float3 colour=lookup.rgb;
	float depth=dlookup.r;
	float dist=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
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
	colour+=skylightTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(colour.rgb,1.f);
}

float4 PS_AtmosOverlayLossPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view=normalize(view);
	float depth=depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float dist=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
	float sine=view.z;
	float2 texc2=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float3 loss=lossTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(loss.rgb,1.f);
}

float4 PS_AtmosOverlayInscPass(atmosVertexOutput IN) : SV_TARGET
{
	float3 view=mul(invViewProj,vec4(IN.pos.xy,1.0,1.0)).xyz;
	view=normalize(view);
	float depth= depthTexture.Sample(clampSamplerState,IN.texCoords.xy).x;
	float dist=depthToDistance(depth,IN.pos.xy,nearZ,farZ,tanHalfFov);
	float sine		=view.z;
	float2 texc2	=float2(pow(dist,0.5f),0.5f*(1.f-sine));
	float4 insc		=inscatterTexture.Sample(clampSamplerState,texc2);
	float cos0		=dot(view,lightDir);
	float3 colour	=InscatterFunction(insc,cos0);
	colour			+=skylightTexture.Sample(clampSamplerState,texc2).rgb;
    return float4(colour.rgb,1.f);
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

technique11 simul_atmospherics_overlay
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(MultiplyBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		//SetBlendState(AddBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
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

