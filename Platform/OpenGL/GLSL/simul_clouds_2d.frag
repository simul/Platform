uniform sampler2D imageTexture;

uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;

uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;

uniform float hazeEccentricity;
uniform float cloudEccentricity;
uniform vec3 mieRayleighRatio;
uniform vec4 lightResponse;
uniform vec3 lightDir;
uniform vec3 eyePosition;
uniform vec3 sunlight;
uniform float maxFadeDistanceMetres;
uniform float cloudInterp;

varying vec2 texc_global;
varying vec2 texc_detail;
varying vec3 wPosition;

#define pi (3.1415926536)
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
		/(vec3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

void main()
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	vec2 fade_texc		=vec2(sqrt(length(difference)/maxFadeDistanceMetres),0.5*(1.0-sine));
	float cos0			=dot(normalize(lightDir),(view));
    // original image
    vec4 c				=texture2D(imageTexture,texc_detail);
    vec4 global_lookup	=mix(texture2D(coverageTexture1,texc_global),texture2D(coverageTexture2,texc_global),cloudInterp);
	float opacity		=clamp(c.a*global_lookup.y,0.0,1.0);
	float light			=HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 final			=sunlight*(lightResponse.w+lightResponse.x*light*exp(-c.r*global_lookup.y*32.0));
	vec3 loss_lookup	=texture2D(lossSampler,fade_texc).rgb;
	vec4 insc_lookup	=texture2D(inscatterSampler,fade_texc);
	final				*=loss_lookup;
	final				+=InscatterFunction(insc_lookup,cos0);
	final				+=texture2D(skylightSampler,fade_texc).rgb;
    gl_FragColor		=vec4(final.rgb,opacity);
}
