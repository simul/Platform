#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

vec4 Clouds2Dunfaded(vec3 view,float cos0,vec2 texc_global,vec2 texc_detail,vec3 wPosition,float dist)
{
    vec4 detail			=texture2D(imageTexture,texc_detail);
    vec4 coverage		=mix(texture2D(coverageTexture1,texc_global),texture2D(coverageTexture2,texc_global),cloudInterp);
	float opacity		=clamp(detail.a*Y(coverage),0.0,1.0);
	if(opacity<=0)
		discard;
	float hg				=HenyeyGreenstein(cloudEccentricity,cos0);
	float scattered_light	=detail.a*exp(-detail.r*coverage.y*32.0);
	vec3 colour				=sunlight*(lightResponse.w+lightResponse.x*hg*scattered_light);
    return vec4(colour,opacity);
}

vec4 Clouds2DPS(vec2 texc_global,vec2 texc_detail,vec3 wPosition,float dist)
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);

	vec4 final			=Clouds2Dunfaded(view,cos0,texc_global,texc_detail,wPosition,dist);
	vec2 fade_texc		=vec2(sqrt(length(difference)/maxFadeDistanceMetres),0.5*(1.0-sine));
	vec3 loss_lookup	=texture_clamp_mirror(lossTexture,fade_texc).rgb;
	vec4 insc_lookup	=texture_clamp_mirror(inscTexture,fade_texc);
	vec4 skyl_lookup	=texture_clamp_mirror(skylTexture,fade_texc);
	final.rgb			*=loss_lookup;
	final.rgb			+=InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final.rgb			+=skyl_lookup.rgb;
	return final;
}

vec4 Clouds2DPS_illum(vec2 texc_global,vec2 texc_detail,vec3 wPosition,float dist)
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);

	vec4 final			=Clouds2Dunfaded(view,cos0,texc_global,texc_detail,wPosition,dist);
	
	final.rgb			=ApplyFade(final.rgb,difference,lightDir,mieRayleighRatio,hazeEccentricity,maxFadeDistanceMetres);
	return final;
}

#endif