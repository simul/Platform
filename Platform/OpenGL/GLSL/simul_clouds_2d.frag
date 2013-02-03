uniform sampler2D imageTexture;
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
uniform float layerDensity;

varying vec2 texc;
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
    vec4 c				=texture2D(imageTexture,texc);
	float opacity		=clamp(layerDensity+c.r-0.5,0,1.0);
	float light			=HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 final			=c.rrr*sunlight*(lightResponse.w+lightResponse.x*light);
	vec3 loss_lookup	=texture2D(lossSampler,fade_texc).rgb;
	vec4 insc_lookup	=texture2D(inscatterSampler,fade_texc);
	final				*=loss_lookup;
	final				+=InscatterFunction(insc_lookup,cos0);
	final				+=texture2D(skylightSampler,fade_texc).rgb;
    gl_FragColor		=vec4(final.rgb,opacity);
}
