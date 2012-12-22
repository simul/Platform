#version 140
#include "simul_inscatter_fns.glsl"
uniform sampler2D inscTexture;
#include "simul_earthshadow_uniforms.glsl"
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D skylightTexture;

uniform vec3 lightDir;

varying vec2 texCoords;

void main()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(imageTexture,texCoords);
	float depth=lookup.a;
	if(depth>=1.0) 
		discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc2).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
	
	vec4 insc=EarthShadowFunction(texc2,view);
	vec4 skyl=texture2D(skylightTexture,texc2);
	
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(insc,cos0);
	colour+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
