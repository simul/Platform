#ifndef EARTH_SHADOW_FADE_SL
#define EARTH_SHADOW_FADE_SL

vec3 ApplyFadeFromTexture(Texture2D lossTexture,Texture2D inscTexture,Texture2D skylTexture,Texture2D illuminationTexture,vec3 colour,vec3 offset,vec3 lightDir,vec3 mieRayleighRatio,float hazeEccentricity,float maxFadeDistMetres)
{
	vec3 view			=normalize(offset);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);

	vec2 fade_texc		=vec2(sqrt(length(offset)/maxFadeDistMetres),0.5*(1.0-sine));

	vec3 loss			=texture_clamp_mirror(lossTexture,fade_texc).rgb;

	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illuminationTexture,illum_texc).rg;

	float light			=saturate((fade_texc.x-nearFarTexc.x)/0.1);

	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	loss				*=light;

	vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 insc			=vec4(insc_far.rgb-insc_near.rgb,insc_far.a);//0.5*(insc_near.a+insc_far.a));

	vec4 skyl_lookup	=texture_clamp_mirror(skylTexture,fade_texc);
	colour.rgb			*=loss;
	colour.rgb			+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour.rgb			+=skyl_lookup.rgb;
	return colour;
}


#endif