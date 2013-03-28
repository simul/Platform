// simul_sky.frag - a GLSL fragment shader
// Copyright 2008-2012 Simul Software Ltd
#include "simul_inscatter_fns.glsl"

uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform vec3 lightDir;
uniform vec3 earthShadowNormal;
uniform float radiusOnCylinder;
uniform float skyInterp;

uniform float maxDistance;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
uniform vec3 cloudOrigin;
uniform vec3 cloudScale;
uniform vec3 viewPosition;
uniform float overcast;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec3 dir;

void main()
{
	vec3 view=normalize(dir);
	float sine=view.z;
	vec2 texc=vec2(1.0,0.5*(1.0-sine));
	vec4 insc=texture2D(inscTexture,texc);
	vec4 skyl=texture2D(skylightTexture,texc);
	float cos0=dot(lightDir.xyz,view.xyz);
	vec3 colour=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
