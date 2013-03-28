#version 140
#include "saturate.glsl"
#include "simul_inscatter_fns.glsl"

#define ALIGN
#define cbuffer layout(std140) uniform
#define R0
#include "../../CrossPlatform/simul_2d_clouds.sl"

uniform sampler2D imageTexture;
uniform sampler2D coverageTexture1;
uniform sampler2D coverageTexture2;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture;
#include "simul_earthshadow_uniforms.glsl"

in vec2 texc_global;
in vec2 texc_detail;
in vec3 wPosition;

void main()
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	vec2 fade_texc		=vec2(sqrt(length(difference)/maxFadeDistanceMetres),0.5*(1.0-sine));
	float cos0			=dot(normalize(lightDir),(view));

    vec4 detail			=texture2D(imageTexture,texc_detail);//vec4(1,1,1,1);//
    vec4 coverage		=mix(texture2D(coverageTexture1,texc_global),texture2D(coverageTexture2,texc_global),cloudInterp);
	float opacity		=clamp(detail.a*coverage.y,0.0,1.0);
	if(opacity<=0)
		discard;
	float hg			=HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 light			=EarthShadowLight(fade_texc,view);
	vec3 final			=sunlight*light*(lightResponse.w+lightResponse.x*hg*detail.a*exp(-detail.r*coverage.y*32.0));
	vec3 loss_lookup	=texture2D(lossTexture,fade_texc).rgb;
	vec4 insc_lookup	=texture2D(inscTexture,fade_texc);
	final				*=loss_lookup;
	final				+=light*InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final				+=texture2D(skylTexture,fade_texc).rgb;
    gl_FragColor		=vec4(final,opacity);
}
