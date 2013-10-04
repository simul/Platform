#version 140
#include "saturate.glsl"
uniform sampler2D rainTexture;

uniform float intensity;
uniform float offset;
uniform vec3 lightColour;
uniform vec3 lightDir;

varying vec3 texc;
varying vec3 view;
#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)*pow(u,-1.5)/(4.0*pi);
}
void main()
{
	vec2 texcoord=texc.xy;
	texcoord.y+=offset;

	float fade=texc.z;
	vec4 lookup1=1.0*fade*texture(rainTexture,texcoord.xy);
	texcoord.xy*=2;
	vec4 lookup2=0.7*fade*texture(rainTexture,texcoord.xy);
	texcoord.xy*=2;
	vec4 lookup3=0.5*fade*texture(rainTexture,texcoord.xy);

	vec4 lookup	=lookup1+lookup2+lookup3;
	float u		=1.0-intensity;
    lookup		=saturate(lookup);
	vec3 v		=normalize(view);
	float cos0	=dot(v,lightDir);
	lookup.a	=lookup.r;//intensity*intensity
	lookup.a	-=u;
	lookup.a	*=0.5;
    lookup		=saturate(lookup);
	lookup.rgb	=lightColour*(1.0+HenyeyGreenstein(0.87,cos0));
    
    gl_FragColor=lookup;
}
