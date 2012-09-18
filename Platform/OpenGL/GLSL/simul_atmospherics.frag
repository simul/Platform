
uniform sampler2D image_texture;
uniform sampler2D loss_texture;
uniform sampler2D insc_texture;

uniform float hazeEccentricity;
uniform vec3 lightDir;
uniform mat4 invViewProj;
uniform vec3 mieRayleighRatio;

varying vec2 texCoords;
const float pi=3.1415926536;

float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return 0.5*0.079577+0.5*(1.0-g2)/(4.0*pi*sqrt(u*u*u));
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
    vec4 lookup=texture2D(image_texture,texCoords);
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(pos*invViewProj).xyz;
	view=abs(normalize(view));
	float sine=view.z;
	float depth=lookup.a;

	if(depth>=1.0) 
		discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture2D(loss_texture,texc2).rgb;
	vec3 colour=lookup.rgb;
//	colour*=loss;
	vec4 inscatter_factor=texture2D(insc_texture,texc2);
	float cos0=dot(view,lightDir);
	colour=InscatterFunction(inscatter_factor,cos0);
    gl_FragColor=vec4(view,1.0);
}
