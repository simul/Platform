#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/earth_shadow.sl"
#include "../../CrossPlatform/SL/earth_shadow_uniforms.sl"
#include "../../CrossPlatform/SL/illumination.sl"
#include "../../CrossPlatform/SL/sky_constants.sl"
uniform sampler2D inscTexture;
uniform sampler2D illuminationTexture;
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
	float alt_km		=eyePosition.z/1000.0;
    gl_FragColor		=OvercastInscatter(inscTexture,illuminationTexture,texCoords,alt_km,maxFadeDistanceKm
								,overcast,overcastBaseKm,overcastRangeKm,targetTextureSize);
}