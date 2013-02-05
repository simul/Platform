#include "simul_inscatter_fns.glsl"
uniform sampler2D imageTexture;

uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;

uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;

uniform float cloudEccentricity;
uniform vec4 lightResponse;
uniform vec3 lightDir;
uniform vec3 eyePosition;
uniform vec3 sunlight;
uniform float maxFadeDistanceMetres;
uniform float cloudInterp;
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;

varying vec2 texc_global;
varying vec2 texc_detail;
varying vec3 wPosition;

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
	final				+=InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final				+=texture2D(skylightSampler,fade_texc).rgb;
    gl_FragColor		=vec4(final.rgb,opacity);
}
