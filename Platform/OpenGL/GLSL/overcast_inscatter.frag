#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/illumination.sl"
#include "../../CrossPlatform/sky_constants.sl"
uniform sampler2D inscTexture;
uniform sampler2D illuminationTexture;
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
    gl_FragColor= OvercastInscatter(inscTexture,illuminationTexture,texCoords,overcast);
}
