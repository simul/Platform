#version 140
#include "saturate.glsl"
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;
#include "../../CrossPlatform/simul_inscatter_fns.sl"
uniform sampler2D depthTexture;
uniform sampler2D lossTexture;
uniform float maxDistance;

// Godrays are cloud-dependent. So we require the cloud texture.
uniform sampler2D cloudShadowTexture;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
uniform vec3 cloudOrigin;
uniform vec3 cloudScale;
uniform vec3 viewPosition;
uniform float overcast;

uniform vec3 lightDir;

varying vec2 texCoords;

#include "view_dir.glsl"

void main()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texCoords);
	float depth=lookup.x;
	vec2 texc=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc).rgb;
    gl_FragColor=vec4(loss,1.0);
}
