#ifndef GODRAYS_SL
#define GODRAYS_SL

vec4 Godrays(Texture2D cloudShadowTexture,Texture2D cloudNearFarTexture,Texture2D inscTexture,Texture2D overcTexture,vec2 pos,mat4 invViewProj,float maxFadeDistance,float solid_dist)
{
	vec3 view			=mul(invViewProj,vec4(pos.xy,1.0,1.0)).xyz;
	view				=normalize(view);
	// Get the direction in shadow space, then scale so that we have unit xy.
	vec3 view_s			=normalize(mul(invShadowMatrix,vec4(view,0.0)).xyz);
	
	vec3 tex_pos		=mul(invShadowMatrix,vec4(view,0.0)).xyz;
	float nearFarTexcX	=atan2(tex_pos.y,tex_pos.x)/(2.0*3.1415926536);//
	vec4 shadowNearFar	=texture_wrap_lod(cloudNearFarTexture,vec2(nearFarTexcX,0.5),0);//vec4(0,1.0,0,1.f);//
	if(shadowNearFar.w<=shadowNearFar.z)
		discard;
#if 0
	vec3 rp				=GetRadialPos(viewPosition+view);
	return vec4(saturate(shadowNearFar.w-shadowNearFar.z),0,0,1.0);
#else
	//view_s				/=length(view_s.xy);
	float sine			=view.z;
	float cos0			=dot(view,lightDir);
	// Now the step value is the ratio to 1 of a unit step in the shadow plane.
	float step			=1.0;///length(view_s.xy);
	vec4 total_insc		=vec4(0,0,0,0);
	float dist_max		=shadowRange/maxFadeDistance;
	float fade_dist_1	=0.0;
	vec2 fade_texc		=vec2(0.0,0.5*(1.0-sine));
	vec2 fade_texc_1	=fade_texc;
	vec2 fade_texc_0	=fade_texc;
	vec2 solid_fade_texc=vec2(pow(solid_dist,.5),0.5*(1.0-sine));
	vec4 insc1			=texture_clamp_mirror(inscTexture,fade_texc)-texture_clamp_mirror(overcTexture,fade_texc);
	vec4 insc_s			=texture_clamp_mirror(inscTexture,solid_fade_texc)-texture_clamp_mirror(overcTexture,solid_fade_texc);
	float ill			=1.0;
	float eff_remaining	=1.0;
	float total_ill		=0.0;
	float r1			=0.0;
	vec4 total_overc	=vec4(0,0,0,0);
	// what proportion of the ray is in light?
	for(int i=0;i<godraysSteps;i++)
	{
		float r0			=r1;
		// we first get the radius on the shadow plane, then convert 
		r1					=godrays_distances[i].x;
		float fade_dist_0	=r0*step*shadowRange/maxFadeDistance;
		fade_dist_1			=r1*step*shadowRange/maxFadeDistance;
		float fade_intro	=saturate((solid_dist-fade_dist_0)/(fade_dist_1-fade_dist_0));
		//if(r0<=shadowNearFar.w&&r1>=shadowNearFar.z)
		{
			float r				=0.5*(r0+r1);
			if(fade_dist_0<solid_dist)
			{
				//fade_dist_1		=min(fade_dist_1,solid_dist);
				float fade_dist		=r*step*shadowRange/maxFadeDistance;
				float eff			=exp(-.1*r1/dist_max);
				fade_texc.x			=sqrt(fade_dist);
				float true_dist		=fade_dist*maxFadeDistance;
			//float near=saturate((solid_dist-fade_dist)*10.0);
				ill					=eff*fade_intro*GetIlluminationAt(cloudShadowTexture,viewPosition+view*true_dist).y;//eff*
	#if 1
				fade_texc_0.x		=sqrt(fade_dist_0);
				fade_texc_1.x		=sqrt(fade_dist_1);
				vec4 insc0			=max(texture_clamp_mirror_lod(inscTexture,fade_texc_0,0)
										-texture_clamp_mirror_lod(overcTexture,fade_texc_0,0)
										,vec4(0,0,0,0));
				insc1				=max(texture_clamp_mirror_lod(inscTexture,fade_texc_1,0)
										-texture_clamp_mirror_lod(overcTexture,fade_texc_1,0)
										,vec4(0,0,0,0));
				vec4 insc_diff		=max(insc1-insc0,vec4(0,0,0,0));
				total_insc			+=insc_diff*ill;
	#endif
				total_ill			+=ill*abs(r1-r0);
			}
		}
	}
#if 0
	total_insc			=insc_s*saturate(total_ill);
#endif
	total_insc.a=1.0;
	vec3 gr				=InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio);
	//colour			+=texture_clamp_mirror(skylTexture,fade_texc).rgb;
	gr					*=exposure;//*(1.0-illum_lookup.z);
	gr					=max(gr,vec3(0.0,0.0,0.0));
    return vec4(gr,1.0);
#endif
}
#endif
