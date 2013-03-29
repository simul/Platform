#version 140
#include "saturate.glsl"
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;
#include "simul_inscatter_fns.glsl"
uniform sampler2D inscTexture;
#include "simul_earthshadow_uniforms.glsl"

uniform sampler2D skylightTexture;
uniform vec3 lightDir;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;

void main()
{
	vec3 view=normalize(dir);
	float sine=view.z;
	// Full brightness:
	vec2 texc2=vec2(1.0,0.5*(1.0-sine));
	vec4 insc=EarthShadowFunction(texc2,view);
	vec4 skyl=texture2D(skylightTexture,texc2);
	float cos0=dot(view,lightDir);
	vec3 colour=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
