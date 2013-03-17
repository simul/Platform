#version 130
uniform mat4 worldViewProj;
in vec3 vertex;
in vec2 in_coord;
out vec3 texc;

void main(void)
{
    gl_Position		=worldViewProj*vec4(vertex,1.0);
    texc.x			=in_coord.x;
    texc.y			=in_coord.x;
    texc.z			=in_coord.y;
}