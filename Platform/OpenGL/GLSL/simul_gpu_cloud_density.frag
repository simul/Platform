#version 140
#include "CppGlsl.hs"
uniform sampler3D volumeNoiseTexture;
#include "../../CrossPlatform/SL/simul_gpu_clouds.sl"
varying vec2 texCoords;

void main()
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(texCoords.xy);
	vec3 noisespace_texcoord	=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	// noise_texel is the size of a noise texel
	float noise_texel			=1.0/noiseDimsZ;
	float height				=noiseScale.z;
	float noise_val				=NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time,height,noise_texel);
	float hm					=humidity*GetHumidityMultiplier(densityspace_texcoord.z);
	float diffusivity			=0.02;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    gl_FragColor				=vec4(dens,0,0,1.0);
}
