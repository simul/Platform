// simul_sky.frag - an OGLSL fragment shader
// Copyright 2008 Simul Software Ltd


uniform sampler2D skyTexture1;
uniform sampler2D skyTexture2;
// the App updates uniforms "slowly" (eg once per frame) for animation.
uniform float hazeEccentricity;
uniform float altitudeTexCoord;
uniform vec3 mieRayleighRatio;
uniform vec3 lightDir;
uniform float skyInterp;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;
#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return 0.5*0.079577+.5*(1.0-g2)/(4.0*pi*pow(u,1.5));
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

void main()
{
	vec3 view=normalize(dir);
	float sine=view.z;
	vec2 texcoord=vec2(0.5*(1.0-sine),altitudeTexCoord);
	vec4 inscatter_factor_1=texture2D(skyTexture1,texcoord);
	vec4 inscatter_factor_2=texture2D(skyTexture2,texcoord);
	vec4 inscatter_factor=mix(inscatter_factor_1,inscatter_factor_2,skyInterp);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec3 colour=InscatterFunction(inscatter_factor,cos0);
    gl_FragColor=vec4(colour.rgb,1.0);
}
