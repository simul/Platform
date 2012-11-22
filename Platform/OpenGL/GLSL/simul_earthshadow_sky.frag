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
uniform float maxFadeDistance;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;
#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*pow(u,1.5));
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
	// Full brightness:
	vec2 texcoord=vec2(1.0,0.5*(1.0-sine));
	vec4 insc=texture2D(inscTexture,texcoord);
	vec4 skyl=texture2D(skylightTexture,texcoord);
	
	// The Earth's shadow: let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	// First get the part of view that is along the light direction
	float along=dot(lightDir,view);
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*lightDir;
	// Now get the part that's on the cylinder radius:
	float on_radius=dot(on_cross_section,earthShadowNormal);
	vec3 on_x=on_cross_section-on_radius*earthShadowNormal;
	float sine_phi=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float s=sine_phi;//clamp(sine_phi,-1.0,0.0);
	float cos2=1.0-s*s;
	// Normalized so that Earth radius is 1.0..
	float u=1.0-radiusOnCylinder*radiusOnCylinder*cos2;
	float d=0.0;
	if(u>=0)
	{
		float L=-radiusOnCylinder*sine_phi;
		if(radiusOnCylinder<1.0)
			L+=sqrt(u);
		else
			L-=sqrt(u);
		L=max(0.0,L);
		float sine_gamma=length(on_cross_section);
		L=L/sine_gamma;
		// L is the distance to the outside of the Earth's shadow in this direction, normalized to the Earth's radius.
		// Renormalize it to the fade distance.
		d=L/maxFadeDistance;
	}
	// Inscatter at distance d
	vec2 texcoord_d=vec2(pow(d,0.5),0.5*(1.0-sine));
	vec4 inscb=texture2D(inscTexture,texcoord_d);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	// but we just use the inscatter to d if we're looking INTO the cylinder.
	if(radiusOnCylinder<1.0||d==0.0)
		insc-=inscb;
	else
		insc=mix(insc,inscb,saturate(-along));
	float cos0=dot(lightDir.xyz,view.xyz);
	vec3 colour=InscatterFunction(insc,cos0);
	colour+=skyl.rgb;
	
    gl_FragColor=vec4(colour.rgb,1.0);
}
