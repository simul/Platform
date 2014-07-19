#version 140
#include "CppGlsl.hs"
uniform sampler3D volumeNoiseTexture;
uniform sampler2D maskTexture;
#include "../../CrossPlatform/SL/gpu_cloud_constants.sl"
#include "../../CrossPlatform/SL/simul_gpu_clouds.sl"
in vec2 texCoords;
out vec4 FragColour;
void main()
{
	float dens					=CloudDensity(volumeNoiseTexture,maskTexture,texCoords,humidity,diffusivity,octaves,persistence,time,zPixel,zSize,noiseDimsZ
					, noiseScale
				   , baseLayer
					 , transition
					 , upperDensity);

    FragColour					=vec4(dens,0,0,1.0);
}
