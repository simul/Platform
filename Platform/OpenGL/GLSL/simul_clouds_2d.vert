#version 140
#define ALIGN
#define cbuffer layout(std140) uniform
#define R0
#include "../../CrossPlatform/simul_2d_clouds.sl"

//uniform Transformation
//{
uniform mat4 projection_matrix;
uniform mat4 modelview_matrix;
//};
	//uniform vec2 origin;
	//uniform float globalScale;
	//uniform float detailScale;


in vec4 vertex;

out vec2 texc_global;
out vec2 texc_detail;
out vec3 wPosition;

void main()
{
   // gl_Position				=ftransform();
	gl_Position				=vec4(vertex.xyz,1.0)*modelview_matrix*projection_matrix;

	vec4 pos				=vertex;
    wPosition				=pos.xyz;
    texc_global				=(pos.xy-origin.xy)/globalScale;
    texc_detail				=(pos.xy-origin.xy)/detailScale;
    
	vec3 eyespacePosition	=(modelview_matrix*pos).xyz;
}
