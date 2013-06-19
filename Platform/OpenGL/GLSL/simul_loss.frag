#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/depth.sl"
uniform sampler2D depthTexture;
uniform sampler2D lossTexture;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.

in vec2 pos;
varying vec2 texc;

#include "view_dir.glsl"

void main()
{
	vec3 view=texCoordToViewDirection(texc);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texc);
	float depth=lookup.x;
	float dist=depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	vec2 fade_texc=vec2(pow(dist,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,fade_texc).rgb;

    gl_FragColor=vec4(loss,1.0);
}
