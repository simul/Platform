#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/solid_constants.sl"

in vec3 vertex;
in vec2 in_coord;
out vec2 texCoords;

void main(void)
{
    gl_Position		=worldViewProj*vec4(vertex,1.0);
    texCoords		=in_coord;
}