#version 130
//#extension GL_EXT_gpu_shader4 : enable
// simul_terrain.frag - a GLSL fragment shader
// Copyright 2012 Simul Software Ltd
uniform vec3 eyePosition;
uniform vec3 lightDir;
uniform vec3 sunlight;
uniform float maxFadeDistanceMetres;
uniform sampler2DArray textures;

float saturate(float f)
{
	return clamp(f,0.0,1.0);
}

vec2 saturate(vec2 f)
{
	return clamp(f,0.0,1.0);
}

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


// varyings are written by vert shader, interpolated, and read by frag shader.
in vec3 wPosition;
in vec2 texcoord;

out vec4 fragmentColour;

void main()
{
	vec4 result;
	vec2 texcoord=vec2(wPosition.xy/2000.0);
	vec4 layer1=texture(textures,vec3(texcoord,1.0));
	vec4 layer2=texture(textures,vec3(texcoord,0.0));
	vec4 texel=mix(layer1,layer2,clamp(wPosition.z/100.0,0.0,1.0));
	result.rgb=texel.rgb*lightDir.z*sunlight;
	// Distance
	result.a=length(wPosition-eyePosition)/maxFadeDistanceMetres;
	result.rgb=result.rgb;
    fragmentColour=result;
}
