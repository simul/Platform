#version 140
#include "CppGlsl.hs"
#include "../../CrossPlatform/earth_shadow.sl"
#include "../../CrossPlatform/illumination.sl"
#include "../../CrossPlatform/sky_constants.sl"
#include "../../CrossPlatform/earth_shadow_uniforms.sl"
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
	float alt_km			=eyePosition.z/1000.0;
	vec2 texc	=vec2(texCoords.x,1.0-texCoords.y);
    gl_FragColor		=IlluminationBuffer(alt_km,texc,targetTextureSize,overcastBaseKm,overcastRangeKm,maxFadeDistanceKm
			,maxFadeDistance,terminatorDistance,radiusOnCylinder,earthShadowNormal,sunDir);
}