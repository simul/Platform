#version 140
#include "CppGlsl.hs"
uniform sampler2D noise_texture;
uniform float persistence;
varying vec2 texCoords;

void main(void)
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=texCoords;
	float mul=.5;
    for(int i=0;i<24;i++)
    {
		vec4 c=texture_clamp_lod(noise_texture,texcoords,0);
		texcoords*=2.0;
		texcoords+=mul*vec2(0.2,0.2)*c.xy;
		result+=mul*c;
		mul*=persistence;
    }
    result.rgb=vec3(1.0,1.0,1.0);//=saturate(result*1.5);
	result.a=saturate(result.a-0.4)/0.4;
    gl_FragColor=result;
}
