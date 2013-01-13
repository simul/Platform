#ifndef SIMUL_INSCATTER_FNS_HLSL
#define SIMUL_INSCATTER_FNS_HLSL

#define pi (3.1415926536)

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}
float3 saturate(float3 x)
{
	return clamp(x,float3(0.0,0.0,0.0),float3(1.0,1.0,1.0));
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)*pow(u,-1.5)/(4.0*pi);
}

float3 InscatterFunction(float4 inscatter_factor,float hazeEccentricity,float cos0,float3 mieRayleighRatio)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	float3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(float3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	float3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}
#endif