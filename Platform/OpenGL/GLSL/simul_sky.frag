// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008 Simul Software Ltd


uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
// the App updates uniforms "slowly" (eg once per frame) for animation.
uniform float hazeEccentricity;
uniform float altitudeTexCoord;
uniform vec3 mieRayleighRatio;
uniform vec3 lightDir;
uniform vec3 earthShadowNormal;
uniform float radiusOnCylinder;
uniform float skyInterp;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;
#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*pow(u,1.5));
	//return 0.5*0.079577+.5*(1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(vec3(BetaRayleigh,BetaRayleigh,BetaRayleigh)+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 insc=BetaTotal*inscatter_factor.rgb;
	return insc;
}

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}
void main()
{
	vec3 view=normalize(dir);
	float sine=view.z;
	vec2 texcoord=vec2(1.0,0.5*(1.0-sine));
	vec4 inscatter_factor=texture2D(inscTexture,texcoord);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec4 skylb=texture2D(skylightTexture,texcoord);
	vec3 colour=InscatterFunction(inscatter_factor,cos0);
	colour+=skylb.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
