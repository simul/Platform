#version 140
#include "CppGlsl.hs"
uniform sampler3D volumeNoiseTexture;
#include "../../CrossPlatform/simul_gpu_clouds.sl"
uniform sampler3D density_texture;
uniform sampler3D light_texture;
uniform sampler3D ambient_texture;

in vec2 texCoords;

void main(void)
{	
	vec3 densityspace_texcoord	=assemble3dTexcoord(texCoords.xy);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=(vec4(densityspace_texcoord,1.0)*transformMatrix).xyz;
	vec2 light_lookup			=saturate(texture(light_texture,lightspace_texcoord).xy);
	vec2 amb_texel				=texture(ambient_texture,ambient_texcoord).xy;
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(texture(density_texture,densityspace_texcoord).x);
    gl_FragColor				=vec4(ambient_lookup,density,light_lookup.x,light_lookup.y);
}
