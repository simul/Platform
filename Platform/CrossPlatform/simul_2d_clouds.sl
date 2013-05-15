#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

vec4 Clouds2DPS(vec2 texc_global,vec2 texc_detail,vec3 wPosition)
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	vec2 fade_texc		=vec2(sqrt(length(difference)/maxFadeDistanceMetres),0.5*(1.0-sine));
	float cos0			=dot(normalize(lightDir),view);
    vec4 detail			=texture2D(imageTexture,texc_detail);
    vec4 coverage		=mix(texture2D(coverageTexture1,texc_global),texture2D(coverageTexture2,texc_global),cloudInterp);
	float opacity		=clamp(detail.a*Y(coverage),0.0,1.0);
	if(opacity<=0)
		discard;
	float hg				=HenyeyGreenstein(cloudEccentricity,cos0);
	vec3 light				=EarthShadowLight(fade_texc,view);
	float scattered_light	=detail.a*exp(-detail.r*coverage.y*32.0);
	vec3 final				=sunlight*light*(lightResponse.w+lightResponse.x*hg*scattered_light);
	vec3 loss_lookup		=texture_clamp_mirror(lossTexture,fade_texc).rgb;
	vec4 insc_lookup		=texture_clamp_mirror(inscTexture,fade_texc);
	vec4 skyl_lookup		=texture_clamp_mirror(skylTexture,fade_texc);
	final					*=loss_lookup;
	final					+=light*InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final					+=skyl_lookup.rgb;
    return vec4(final,opacity);
}

#endif