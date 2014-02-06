#ifndef SIMUL_INSCATTER_FNS_HLSL
#define SIMUL_INSCATTER_FNS_HLSL

#ifndef pi
#define pi (3.1415926536)
#endif

#ifndef RAYLEIGH_BETA_FACTOR
#define RAYLEIGH_BETA_FACTOR (0.0596831)
#endif

float CalcRayleighBeta(float cos0)
{
	return (1.0+cos0*cos0); // Factor of (0.0596831) is applied in texture generation now.
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)*pow(u,-1.5)/(4.0*pi);
}

vec3 PrecalculatedInscatterFunction(vec4 inscatter_factor,float BetaRayleigh,float BetaMie,vec3 mieRayleighRatio)
{
	vec3 BetaTotal	=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour		=BetaTotal*inscatter_factor.rgb;
	return colour;
}

vec3 InscatterFunction(vec4 inscatter_factor,float hazeEccentricity,float cos0,vec3 mieRayleighRatio)
{
	float BetaRayleigh	=CalcRayleighBeta(cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	return PrecalculatedInscatterFunction(inscatter_factor,BetaRayleigh,BetaMie,mieRayleighRatio);
}

#endif