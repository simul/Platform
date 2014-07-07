#version 140
#include "CppGlsl.hs"
uniform sampler3D volumeNoiseTexture;
uniform sampler2D input_light_texture;
uniform sampler3D density_texture;
#include "../../CrossPlatform/SL/gpu_cloud_constants.sl"
#include "../../CrossPlatform/SL/simul_gpu_clouds.sl"

varying vec2 texCoords;

void main(void)
{
	vec2 texcoord				=texCoords.xy;//+texCoordOffset;
	vec4 previous_light			=texture(input_light_texture,texcoord.xy);
	vec3 lightspace_texcoord	=vec3(texcoord.xy,zPosition);
	vec3 densityspace_texcoord	=(vec4(lightspace_texcoord,1.0)*transformMatrix).xyz;
	float density				=texture(density_texture,densityspace_texcoord).x;
	float direct_light			=previous_light.x*exp(-extinctions.x*density*stepLength);
	float indirect_light		=previous_light.y*exp(-extinctions.y*density*stepLength);
    gl_FragColor				=vec4(direct_light,indirect_light,1,1);
}
