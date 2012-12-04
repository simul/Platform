#version 140
#include "simul_inscatter_fns.glsl"
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;

uniform vec3 lightDir;
uniform mat4 invViewProj;
varying vec2 texCoords;

void main()
{
    vec4 lookup=texture(imageTexture,texCoords);
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(invViewProj*pos).xyz;
	view=normalize(view);
	float sine=view.z;
	float depth=lookup.a;
	if(depth>=1.0) 
		discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc2).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
	vec4 insc=texture(inscTexture,texc2);
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(insc,cos0);
	vec3 skyl=texture(skylightTexture,texc2).rgb;
	colour+=skyl;
    gl_FragColor=vec4(colour,1.0);
}
