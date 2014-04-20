#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/simul_cloud_constants.sl"
uniform sampler3D cloudDensity;
varying vec2 texCoords;

#define CROSS_SECTION_STEPS 32
void main(void)
{
	vec3 texc=crossSectionOffset+vec3(texCoords.x,yz*texCoords.y,(1.0-yz)*texCoords.y);
	int i=0;
	vec3 accum=vec3(0.0,0.5,1.0);
	for(i=0;i<CROSS_SECTION_STEPS;i++)
	{
		vec4 density=texture3D(cloudDensity,texc);
		vec3 colour=vec3(.5,.5,.5)*(lightResponse.x*density.z+lightResponse.y*density.w);
		colour.gb+=vec2(.125,.25)*(lightResponse.z*density.x);
		float opacity=density.y;
		colour*=opacity;
		accum*=1.0-opacity;
		accum+=colour;
		texc.y=texc.y+1.0/float(CROSS_SECTION_STEPS)*(1.0-yz);
		texc.z=texc.z+1.0/float(CROSS_SECTION_STEPS)*yz;
	}
	//accum=texture3D(cloud_density,vec3(crossSectionOffset+texCoords.x,crossSectionOffset+texCoords.y,0.35)).rgb;
    gl_FragColor=vec4(accum,1);
}
