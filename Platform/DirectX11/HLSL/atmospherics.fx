#include "AtmosphericsUniforms.hlsl"
float4x4 invViewProj;

Texture2D depthTexture;
Texture2D imageTexture;
Texture2D lossTexture1;
Texture2D inscatterTexture1;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

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
	float3 view=mul(invViewProj,pos).xyz;
	view=normalize(view);
	float4 lookup=imageTexture.Sample(samplerState,IN.texCoords.xy);
	float3 colour=lookup.rgb;
	float depth=lookup.a;
	if(depth>=1.f)
		discard;
#ifdef Y_VERTICAL
	float sine=view.y;
#else
	float sine=view.z;
#endif
	float maxd=1.0;//tex2D(distance_texture,texc2).x;
	float2 texc2=float2(pow(depth/maxd,0.5f),0.5f*(1.f-sine));
	float3 loss=lossTexture1.Sample(samplerState,texc2).rgb;
	colour*=loss;
	float4 inscatter_factor=inscatterTexture1.Sample(samplerState,texc2);
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(inscatter_factor,cos0);

    return float4(colour,1.f);
}
DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = LESS_EQUAL;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

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
