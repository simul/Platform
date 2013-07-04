#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

vec3 ApplyEarthshadowFade(vec3 colour,vec3 offset,vec3 lightDir,vec3 mieRayleighRatio,float hazeEccentricity,float maxFadeDistMetres)
{
	vec3 view			=normalize(offset);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);

	vec2 fade_texc		=vec2(sqrt(length(offset)/maxFadeDistMetres),0.5*(1.0-sine));


	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illuminationTexture,illum_texc).rg;

	float light			=saturate((fade_texc.x-nearFarTexc.x)/0.1);
	
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	vec3 loss			=texture_clamp_mirror(lossTexture,fade_texc).rgb;
	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 skyl_lookup	=texture_clamp_mirror(skylTexture,fade_texc);
	vec4 insc			=insc_far-insc_near;

	loss				*=light;
	colour.rgb			*=loss;
	colour.rgb			+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour.rgb			+=skyl_lookup.rgb;
	return colour;
}

vec3 ApplySimpleFade(vec3 colour,vec3 offset,vec3 lightDir,vec3 mieRayleighRatio,float hazeEccentricity,float maxFadeDistMetres)
{
	vec3 view		=normalize(offset);
	float sine		=view.z;
	float cos0		=dot(normalize(lightDir),view);
	vec2 fade_texc	=vec2(sqrt(length(offset)/maxFadeDistanceMetres),0.5*(1.0-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,fade_texc).rgb;
	vec4 insc		=texture_clamp_mirror(inscTexture,fade_texc);
	vec4 skyl		=texture_clamp_mirror(skylTexture,fade_texc);
	colour			*=loss;
	colour			+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour			+=skyl.rgb;
	return colour;
}

vec4 Clouds2Dunfaded(vec3 view,float cos0,vec2 texc_global,vec2 texc_detail,vec3 wPosition,float dist)
{
    vec4 detail			=texture2D(imageTexture,texc_detail);
    vec4 coverage		=mix(texture2D(coverageTexture1,texc_global),texture2D(coverageTexture2,texc_global),cloudInterp);
	float opacity		=clamp(detail.a*Y(coverage),0.0,1.0);
	if(opacity<=0)
		discard;
	float hg				=HenyeyGreenstein(.8,cos0);
	float scattered_light	=detail.a*exp(-detail.r*Y(coverage)*32.0);
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
	final.rgb			=ApplySimpleFade(final.rgb,difference,lightDir,mieRayleighRatio,hazeEccentricity,maxFadeDistanceMetres);
	return final;
}

vec4 Clouds2DPS_illum(vec2 texc_global,vec2 texc_detail,vec3 wPosition,float dist)
{
	vec3 difference		=wPosition-eyePosition;
	vec3 view			=normalize(difference);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);
	vec4 final			=Clouds2Dunfaded(view,cos0,texc_global,texc_detail,wPosition,dist);
final.rgb			=ApplyEarthshadowFade(final.rgb,difference,lightDir,mieRayleighRatio,hazeEccentricity,maxFadeDistanceMetres);
	return final;
}

#endif