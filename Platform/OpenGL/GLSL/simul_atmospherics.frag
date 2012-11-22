#version 140
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;

uniform float hazeEccentricity;
uniform vec3 lightDir;
uniform mat4 invViewProj;
uniform vec3 mieRayleighRatio;
varying vec2 texCoords;
uniform float directLightMultiplier;
const float pi=3.1415926536;

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return (1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1,1,1)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

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
	colour+=directLightMultiplier*InscatterFunction(insc,cos0);
	vec4 skyl=texture(skylightTexture,texc2);
	colour.rgb+=skyl.rgb;
    gl_FragColor=vec4(colour.rgb,1.0);
}
