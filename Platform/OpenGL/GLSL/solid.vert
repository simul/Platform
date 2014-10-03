#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/solid_constants.sl"

in vec3 vertex;
in vec2 in_coord;
in vec3 in_normal;

out vec2 texCoords;
out vec3 normal;

void main(void)
{
    gl_Position		=worldViewProj*vec4(vertex.xyz,1.0);
    texCoords		=in_coord;
    normal			=in_normal;
}

