#version 140
// simul_planet.frag - a GLSL fragment shader
// Copyright 2013 Simul Software Ltd
#include "CppGlsl.hs"
#include "../../CrossPlatform/sky_constants.sl"
uniform sampler2D planetTexture;
// inputs are written by vert shader, interpolated, and read by frag shader.
in vec2 texcoord;
out vec4 gl_FragColor;

float approx_oren_nayar(float roughness,vec3 view,vec3 normal,vec3 lightDir)
{
	float roughness2 = roughness * roughness;
	vec2 r2=vec2(roughness2,roughness2);
	vec2 oren_nayar_fraction = r2/(r2+ vec2(0.33, 0.09));
	vec2 oren_nayar = vec2(1, 0) + vec2(-0.5, 0.45) * oren_nayar_fraction;
	// Theta and phi
	vec2 cos_theta = saturate(vec2(dot(normal, lightDir), dot(normal, view)));
	vec2 cos_theta2 = cos_theta * cos_theta;
	float u=saturate((1.0-cos_theta2.x) * (1.0-cos_theta2.y));
	float sin_theta = sqrt(u);
	vec3 light_plane = normalize(lightDir - cos_theta.x * normal);
	vec3 view_plane = normalize(view - cos_theta.y * normal);
	float cos_phi = saturate(dot(light_plane, view_plane));
	// Composition
	float diffuse_oren_nayar = cos_phi * sin_theta / max(0.00001,max(cos_theta.x, cos_theta.y));
	float diffuse = cos_theta.x * (oren_nayar.x + oren_nayar.y * diffuse_oren_nayar);
	return diffuse;
}

void main()
{
	vec4 result=texture(planetTexture,texcoord);
	vec3 normal;
	normal.xy=-2.0*(texcoord-vec2(0.5,0.5));
	float l=length(normal.xy);
	if(l>1.0)
		discard;
	normal.z=-sqrt(1.0-l*l);
	float light=approx_oren_nayar(0.2,vec3(0,0,1.0),normal.xyz,lightDir.xyz);
	result.rgb*=colour.rgb;
	result.rgb*=light;
	result.a*=saturate((0.99-l)/0.01);
	result.rgb*=result.a;
    gl_FragColor=result;
}
