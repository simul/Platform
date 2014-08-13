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
	vec3 mr=mieRayleighRatio.xyz;//*inscatter_factor.a
	vec3 BetaTotal	=(BetaRayleigh+BetaMie*mr)/(vec3(1.0,1.0,1.0)+mr);
	vec3 colour		=BetaTotal*inscatter_factor.rgb;
	return colour;
}

vec3 InscatterFunction(vec4 inscatter_factor,float hazeEccentricity,float cos0,vec3 mieRayleighRatio)
{
	float BetaRayleigh	=CalcRayleighBeta(cos0);
	float BetaMie		=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	return PrecalculatedInscatterFunction(inscatter_factor,BetaRayleigh,BetaMie,mieRayleighRatio);
}


vec4 RainbowAndCorona(Texture2D rainbowLookupTexture,Texture2D coronaLookupTexture,float dropletRadius,
					  float rainbowIntensity,vec3 view,vec3 lightDir,vec2 texCoords)
{
	//return texture_clamp(coronaLookupTexture,IN.texCoords.xy);
	 //note: use a float for d here, since a half corrupts the corona
	float d=  -dot( lightDir,normalize(view ) 	);

	vec4 scattered	=texture_clamp(rainbowLookupTexture, vec2( dropletRadius, d));
	vec4 moisture	=1.0;//texture_clamp(moistureTexture,IN.texCoords);

	//(1 + d) will be clamped between 0 and 1 by the texture sampler
	// this gives up the dot product result in the range of [-1 to 0]
	// that is to say, an angle of 90 to 180 degrees
	vec4 coronaDiffracted = texture_clamp(coronaLookupTexture, vec2(dropletRadius, 1.0 + d));
	return (coronaDiffracted + scattered)*rainbowIntensity*moisture.x;
}
#endif