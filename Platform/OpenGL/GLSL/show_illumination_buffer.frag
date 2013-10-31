#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/illumination.sl"
uniform sampler2D illTexture;
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
	
    gl_FragColor=//texture_cmc_lod(illTexture,texCoords,0);
	ShowIlluminationBuffer(illTexture,texCoords);
}
