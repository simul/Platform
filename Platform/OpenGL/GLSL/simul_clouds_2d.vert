#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_2d_clouds.sl"

in vec4 vertex;

out vec2 texc_global;
out vec2 texc_detail;
out vec3 wPosition;

void main()
{
	gl_Position				=projection_matrix*modelview_matrix*vec4(vertex.xyz,1.0);

	vec4 pos				=vertex;
    wPosition				=pos.xyz;
    texc_global				=(pos.xy-origin.xy)/globalScale;
    texc_detail				=(pos.xy-origin.xy)/detailScale;
    
	vec3 eyespacePosition	=(modelview_matrix*pos).xyz;
}
