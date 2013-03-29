#version 140
uniform vec3 sunDir;
#include "saturate.glsl"
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;
#include "simul_inscatter_fns.glsl"
uniform sampler2D inscTexture;
#define DEF_ES
uniform vec3 earthShadowNormal;
uniform float radiusOnCylinder;
uniform float maxFadeDistance;
uniform float terminatorCosine;

#include "simul_earthshadow_uniforms.glsl"
uniform sampler2D skylightTexture;
uniform vec3 lightDir;
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
#include "view_dir.glsl"

varying vec2 texCoords;

void main()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(imageTexture,texCoords);
	float depth=lookup.a;
	if(depth>=1.0) 
		discard;
	vec2 texc=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
	
	vec2 texc2=vec2(1.0,0.5*(1.0-sine));
	vec4 insc=EarthShadowFunction(texc,view);
	vec4 skyl=texture2D(skylightTexture,texc);
	
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
