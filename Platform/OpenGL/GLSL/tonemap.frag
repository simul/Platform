#version 140

#include "CppGlsl.hs"

layout(std140) uniform HdrConstants
{
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec2 offset;
	uniform float nearZ,farZ;
	uniform vec2 tanHalfFov;
	uniform float exposure;
	uniform float gamma;
	uniform vec3 depthToLinFadeDistParams;
	uniform float padHdrConstants;
	uniform vec4 warpHmdWarpParam;
	uniform vec2 lowResTexelSizeX;
	uniform vec2 warpLensCentre;
	uniform vec2 warpScreenCentre;
	uniform vec2 warpScale;
	uniform vec2 warpScaleIn;
	uniform vec2 padHdrConstants2;
	uniform vec4 hiResToLowResTransformXYWH;
} hdrConstants;

uniform sampler2D image_texture;
uniform sampler2D glowTexture;
//uniform float exposure;
//uniform float gamma;
in vec2 texCoords;

void main(void)
{
    // original image
    vec4 c = texture2D(image_texture,texCoords);
    // exposure
	c.rgb*=hdrConstants.exposure;

	vec4 glow=texture(glowTexture,texCoords);
	//c.rgb+=glow.rgb;

    // gamma correction
	//../c.rgb = pow(c.rgb,vec3(gamma,gamma,gamma));
	//c.a=1.0-pow(1.0-c.a,gamma);
    gl_FragColor=c;
}
