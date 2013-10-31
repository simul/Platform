#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/noise.sl"
varying vec2 texCoords;

void main(void)
{
    vec4 c=vec4(rand(texCoords),rand(1.7*texCoords),rand(0.11*texCoords),rand(513.1*texCoords));
    gl_FragColor=c;
}
