#ifndef GODRAYS_SL
#define GODRAYS_SL

vec4 Godrays(Texture2D inscTexture,Texture2D overcTexture,vec2 pos,mat4 invViewProj,float maxFadeDistance,float solid_dist)
{
	vec3 view			=mul(invViewProj,vec4(pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	// Get the direction in shadow space, then scale so that we have unit xy.
	vec3 view_s			=normalize(mul(invShadowMatrix,vec4(view,0.0)).xyz);
	//view_s				/=length(view_s.xy);
	float sine			=view.z;
	float cos0			=dot(view,lightDir);
	// Now the step value is the ratio to 1 of a unit step in the shadow plane.
	float step			=1.0;///length(view_s.xy);
	vec4 total_insc		=vec4(0,0,0,0);
	float dist_max		=shadowRange/maxFadeDistance;
	float fade_dist_1	=0.0;
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));
	vec2 solid_fade_texc=vec2(pow(solid_dist,.5),0.5*(1.0-sine));
	vec4 insc1			=texture_clamp_mirror(inscTexture,fade_texc)-texture_clamp_mirror(overcTexture,fade_texc);
	vec4 insc_s			=texture_clamp_mirror(inscTexture,solid_fade_texc)-texture_clamp_mirror(overcTexture,solid_fade_texc);
	float ill			=1.0;
	float eff_remaining	=1.0;
	float total_ill		=0.0;
	// what proportion of the ray is in light?
	for(int i=0;i<godraysSteps;i++)
	{
		float fade_dist_0	=fade_dist_1;
		// we first get the radius on the shadow plane, then convert 
		float r1		=godrays_distances[i];
		fade_dist_1		=r1*step*shadowRange/maxFadeDistance;
		if(fade_dist_0<solid_dist)
		{
			//fade_dist_1		=min(fade_dist_1,solid_dist);
		float fade_dist	=0.5*(fade_dist_0+fade_dist_1);
			float eff			=exp(-.1*r1/dist_max);
			fade_texc.x			=sqrt(fade_dist);
			float true_dist		=fade_dist*maxFadeDistance;
		//float near=saturate((solid_dist-fade_dist)*10.0);
			ill					=GetIlluminationAt(cloudShadowTexture,viewPosition+view*true_dist).y;//eff*
#if 0
			vec4 insc0			=insc1;
			insc1				=max(texture_clamp_mirror(inscTexture,fade_texc)
									-texture_clamp_mirror(overcTexture,fade_texc)
									,vec4(0,0,0,0));
			vec4 insc_diff		=max(insc1-insc0,vec4(0,0,0,0));
			total_insc			+=insc_diff*ill;
#endif
			total_ill			+=ill;//*abs(fade_dist_1-fade_dist_0);
		}
	}
#if 1
	total_insc			=insc_s*saturate(total_ill);
#endif
	total_insc.a=1.0;
	vec3 gr				=InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio);
	//colour			+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	gr					*=exposure;//*(1.0-illum_lookup.z);
	gr					=max(gr,vec3(0.0,0.0,0.0));
    return vec4(gr,1.0);
}
#endif
