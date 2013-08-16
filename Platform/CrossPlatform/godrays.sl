#ifndef GODRAYS_SL
#define GODRAYS_SL


vec4 Godrays(Texture2D inscTexture,Texture2D overcTexture,vec2 pos,mat4 invViewProj,float maxDistance,float solid_dist)
{
	vec3 view			=mul(invViewProj,vec4(pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	float sine			=view.z;
	float cos0			=dot(view,lightDir);

	vec4 total_insc		=vec4(0,0,0,0);
	#define NUM_STEPS 25
	float retain		=(float(NUM_STEPS)-1.0)/float(NUM_STEPS);
	float dist_max		=shadowRange/maxDistance;
	float dist_1		=0.0;
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));
	vec2 solid_fade_texc=vec2(pow(solid_dist,.5)	,0.5*(1.0-sine));
	//vec2 illum_texc	=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	//vec4 illum_lookup	=texture_wrap_mirror(illuminationTexture,illum_texc);
	vec4 insc1			=texture_clamp_mirror(inscTexture,fade_texc)-texture_clamp_mirror(overcTexture,fade_texc);
	vec4 insc_s			=texture_clamp_mirror(inscTexture,solid_fade_texc)-texture_clamp_mirror(overcTexture,solid_fade_texc);
	float ill			=1.0;
	float eff_remaining	=1.0;
	float total_ill		=0.0;
	// what proportion of the ray is in light?
	for(int i=0;i<godraysSteps;i++)
	{
		float dist_0	=dist_1;
		if(i<godraysSteps&&dist_0<solid_dist)
		{
			dist_1			=godrays_distances[i]/maxDistance;
			dist_1			=min(dist_1,solid_dist);
			float dist		=0.5*(dist_0+dist_1);
			float eff		=exp(-dist/dist_max);
			fade_texc.x		=pow(dist_0,.5);
			float d			=dist*maxDistance;
			ill				=GetIlluminationAt(cloudShadowTexture,viewPosition+view*d).y;//eff*
		/*	vec4 insc0		=insc1;
			insc1			=max(texture_clamp_mirror(inscTexture,fade_texc)
								-texture_clamp_mirror(overcTexture,fade_texc),vec4(0,0,0,0));
			vec4 insc_diff	=max(insc1-insc0,vec4(0,0,0,0));
			total_insc		+=insc_diff*ill;*/
		total_ill+=ill*abs(dist_1-dist_0);
		}
	}
	total_insc	=insc_s*total_ill/1.0;
	vec3 gr				=total_ill;//InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio);
	//colour			+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	gr					*=exposure;//*(1.0-illum_lookup.z);
	gr					=max(gr,vec3(0.0,0.0,0.0));

    return vec4(gr,1.0);
}
#endif
