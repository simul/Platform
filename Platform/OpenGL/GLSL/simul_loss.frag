#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/SL/atmospherics_constants.sl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/depth.sl"
uniform sampler2D depthTexture;
uniform sampler2D lossTexture;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.

in vec2 pos;
varying vec2 texCoords;

#include "view_dir.glsl"

void main()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texCoords);
	float depth=lookup.x;
	float dist=depthToFadeDistance(depth,pos.xy,depthToLinFadeDistParams,tanHalfFov);
	vec2 fade_texc=vec2(pow(dist,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,fade_texc).rgb;

    gl_FragColor=vec4(loss,1.0);
}
