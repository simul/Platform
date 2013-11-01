#ifndef SIMUL_2D_CLOUDS_SL
#define SIMUL_2D_CLOUDS_SL

float NoiseFunction(Texture2D noiseTexture,vec2 pos,float octaves,float persistence,float time)
{
	float dens=0.0;
	float mult=0.5;
	float sum=0.0;
	float t=time;
	int i_oct=int(octaves+1.f);
	for(int i=0;i<5;i++)
	{
		if(i>=i_oct)
			break;
		float val=2.0*texture_wrap(noiseTexture,pos).x-1.0;
		dens+=mult*val;
		sum+=mult;
		mult*=persistence;
		pos*=2.0;
		t*=2.0;
	}
	dens/=sum;
	dens+=0.5;
	return saturate(dens);
}

vec4 Coverage(vec2 texCoords,float humidity,float diffusivity,float octaves,float persistence,float time,Texture2D noiseTexture,float noiseTextureScale)
{
	float noise_val			=NoiseFunction(noiseTexture,texCoords/noiseTextureScale,octaves,persistence,time);
	float dens				=saturate((noise_val+humidity-1.0)/diffusivity);
    return vec4(dens,dens,dens,dens);
}

vec3 ApplyEarthshadowFade(Texture2D illuminationTexture
						,Texture2D lossTexture
						,Texture2D inscTexture
						,Texture2D skylTexture
						,vec3 colour
						,vec3 wEyeToPos
						,vec3 lightDir
						,vec3 mieRayleighRatio,float hazeEccentricity,float maxFadeDistMetres)
{
	vec3 view			=normalize(wEyeToPos);
	float sine			=view.z;
	float cos0			=dot(normalize(lightDir),view);

	vec2 fade_texc		=vec2(sqrt(length(wEyeToPos)/maxFadeDistMetres),0.5*(1.0-sine));

	vec2 illum_texc		=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc	=texture_wrap_mirror(illuminationTexture,illum_texc).xy;

	float light			=saturate((fade_texc.x-nearFarTexc.x)/0.1);
	
	vec2 near_texc		=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc		=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	vec3 loss			=texture_cmc_lod(lossTexture,fade_texc,0).rgb;
	//vec4 insc_far		=texture_clamp_mirror(inscTexture,far_texc);
	//vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 skyl_lookup	=texture_cmc_lod(skylTexture,fade_texc,0);
	//vec4 insc			=vec4(insc_far.rgb-insc_near.rgb,insc_far.a);//0.5*(insc_near.a+insc_far.a));
	vec4 insc			=light*texture_cmc_lod(inscTexture,fade_texc,0);
	//loss				*=light;
	colour.rgb			*=loss;
	colour.rgb			+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour.rgb			+=skyl_lookup.rgb;
	return colour;
}

vec3 InscatterFunction2(vec4 inscatter_factor,float hazeEccentricity,float cos0,vec3 mieRayleighRatio)
{
	float BetaRayleigh=CalcRayleighBeta(cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz);
		//(vec3(1.0,1.0,1.0)+0*inscatter_factor.a*mieRayleighRatio.xyz);*/
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

vec3 ApplySimpleFade(Texture2D lossTexture
						,Texture2D inscTexture
						,Texture2D skylTexture
						,vec3 colour,vec3 wEyeToPos,vec3 lightDir,vec3 mieRayleighRatio,float hazeEccentricity,float maxFadeDistMetres)
{
	vec3 view		=normalize(wEyeToPos);
	float sine		=view.z;
	float cos0		=dot(normalize(lightDir),view);
	vec2 fade_texc	=vec2(sqrt(length(wEyeToPos)/maxFadeDistMetres),0.5*(1.0-sine));
	vec3 loss		=texture_clamp_mirror(lossTexture,fade_texc).rgb;
	vec4 insc		=texture_clamp_mirror(inscTexture,fade_texc);
	vec4 skyl		=texture_clamp_mirror(skylTexture,fade_texc);
	colour			*=loss;
	colour			+=InscatterFunction2(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour			+=skyl.rgb;
	return colour;
}

vec4 Clouds2Dunfaded(Texture2D imageTexture
						,Texture2D coverageTexture
						,float cos0,vec2 texc_global,vec2 texc_detail,vec3 sunlight,vec3 amb,vec4 lightResponse)
{
    vec4 coverage			=texture_wrap(coverageTexture,texc_global);
    vec4 detail				=texture_wrap(imageTexture,texc_detail);
	float opacity			=saturate(detail.a*2.0*Y(coverage)+2.0*Y(coverage)-1.0);
	if(opacity<=0)
		discard;
	float light				=exp(-detail.r*extinction);
	float hg				=HenyeyGreenstein(cloudEccentricity,cos0);
	float scattered_light	=light;//detail.a*exp(-light*Y(coverage)*32.0);
	vec3 colour				=sunlight*(lightResponse.y+lightResponse.x*hg)*scattered_light+amb;
    return					vec4(colour.rgb,opacity);
}

vec4 Clouds2DPS(Texture2D imageTexture,Texture2D coverageTexture
						,Texture2D lossTexture
						,Texture2D inscTexture
						,Texture2D skylTexture
						,vec2 texc_global,vec2 texc_detail,vec3 wEyeToPos
						,vec3 sunlight
						,vec3 amb
						,vec3 lightDir,vec4 lightResponse)
{
	vec3 view	=normalize(wEyeToPos);
	float cos0	=dot(normalize(lightDir),view);
	vec4 final	=Clouds2Dunfaded(imageTexture,coverageTexture,cos0,texc_global,texc_detail,sunlight,amb,lightResponse);
	final.rgb	=ApplySimpleFade(lossTexture
						,inscTexture
						,skylTexture
						,final.rgb,wEyeToPos,lightDir,mieRayleighRatio,hazeEccentricity,maxFadeDistanceMetres);
	return		final;
}

vec4 Clouds2DPS_illum(Texture2D imageTexture,Texture2D coverageTexture
						,Texture2D illuminationTexture
						,Texture2D lossTexture
						,Texture2D inscTexture
						,Texture2D skylTexture
						,vec2 texc_global,vec2 texc_detail,vec3 wEyeToPos
						,vec3 sun_irr,vec3 moon_irr,vec3 amb,vec3 lightDir,vec4 lightResponse)
{
	
    vec4 coverage			=texture_wrap(coverageTexture,texc_global);
    vec4 detail				=texture_wrap(imageTexture,texc_detail);
	float opacity			=saturate(detail.a*2.0*Y(coverage)+2.0*Y(coverage)-1.0);
	if(opacity<=0)
		discard;
	vec3 view	=normalize(wEyeToPos);
	float cos0	=dot(normalize(lightDir),view);
	float scattered_light	=exp(-detail.r*extinction);
	float hg				=HenyeyGreenstein(cloudEccentricity,cos0);

	float sine				=view.z;

	vec2 fade_texc			=vec2(sqrt(length(wEyeToPos)/maxFadeDistanceMetres),0.5*(1.0-sine));

	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec2 nearFarTexc		=texture_wrap_mirror(illuminationTexture,illum_texc).xy;

	float visible_light		=saturate((fade_texc.x-nearFarTexc.x)/0.1);
	
	vec2 near_texc			=vec2(min(nearFarTexc.x,fade_texc.x),fade_texc.y);
	vec2 far_texc			=vec2(min(nearFarTexc.y,fade_texc.x),fade_texc.y);

	vec3 loss				=texture_cmc_lod(lossTexture,fade_texc,0).rgb;
	vec4 insc_far			=texture_clamp_mirror(inscTexture,far_texc);
	vec4 insc_near		=texture_clamp_mirror(inscTexture,near_texc);
	vec4 skyl_lookup		=texture_cmc_lod(skylTexture,fade_texc,0);
	vec4 insc				=vec4(insc_far.rgb-insc_near.rgb,0.5*(insc_near.a+insc_far.a));
	//vec4 insc				=texture_cmc_lod(inscTexture,fade_texc,0);
	insc.rgb				*=visible_light;
	vec3 light				=sun_irr*visible_light+moon_irr;
	vec4 colour				=vec4(light*(lightResponse.y+lightResponse.x*hg)*scattered_light+amb,opacity);
	colour.rgb				*=loss;
	colour.rgb				+=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour.rgb				+=skyl_lookup.rgb;
	return colour;
}

vec4 ShowDetailTexture(Texture2D imageTexture,vec2 texCoords,vec3 sunlight,vec4 lightResponse)
{
	texCoords+=0.5;
    vec4 detail				=texture_wrap(imageTexture,texCoords);
	float opacity			=saturate(detail.a);
	vec3 colour				=vec3(0.5,0.5,1.0);
	if(opacity<=0)
		return vec4(colour,1.0);
	float light				=exp(-detail.r*extinction);
	float scattered_light	=light;//detail.a*exp(-light*Y(coverage)*32.0);
	colour					*=1.0-opacity;
	colour					+=opacity*sunlight*(lightResponse.y+lightResponse.x)*scattered_light;
	//colour=detail.r*.1;
    return vec4(colour,1.0);
}
#endif