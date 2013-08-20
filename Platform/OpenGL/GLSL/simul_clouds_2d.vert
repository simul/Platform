#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_2d_clouds.hs"

in vec4 vertex;

out vec2 texc_global;
out vec2 texc_detail;
out vec4 clip_pos;
out vec3 wPosition;

void main()
{
	clip_pos				=worldViewProj*vec4(vertex.xyz,1.0);
	gl_Position				=clip_pos;
	vec4 pos				=vertex;
    wPosition				=pos.xyz;
    texc_global				=(pos.xy-origin.xy)/globalScale;
    texc_detail				=(pos.xy-origin.xy)/detailScale;
}
