#version 130
uniform mat4 worldViewProj;
in vec3 vertex;
in vec3 in_coord;
out vec3 texc;
out float brightness;

void main(void)
{
    gl_Position		=worldViewProj*vec4(vertex,1.0);
    texc.x			=in_coord.x;
    texc.y			=in_coord.y;
    texc.z			=in_coord.z;
	brightness		=in_coord.z;
}