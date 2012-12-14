#ifndef SIMUL_INSCATTER_FNS_GLSL
#define SIMUL_INSCATTER_FNS_GLSL

uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;
uniform mat4 invViewProj;
#define pi (3.1415926536)

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}
vec3 saturate(vec3 x)
{
	return clamp(x,vec3(0.0,0.0,0.0),vec3(1.0,1.0,1.0));
}

vec3 texCoordToViewDirection(vec2 texCoords)
{
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(invViewProj*pos).xyz;
	view=normalize(view);
	return view;
}

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)*pow(u,-1.5)/(4.0*pi);
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}
#endif