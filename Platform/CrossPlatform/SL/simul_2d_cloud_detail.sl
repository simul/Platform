//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL

vec4 DetailDensity(vec2 texcoords,Texture2D imageTexture,float amplitude,float density,float diffusivity,float foctaves)
{
	float dens				=0.0;
	float mult				=0.5;
	float current_phase		=phase;
	int ioctaves			=int(foctaves)+1;
    for(int i=0;i<ioctaves;i++)//
    {
		vec4 c				=saturate(1.5*texture_wrap(imageTexture,texcoords)-0.5);
		vec2 u				=saturate(cos(2.0*3.14159*texcoords.xy+current_phase)/(1.515+7.135*c.a)+c.a);//(c+current_phase));
		mult				*=saturate(foctaves-i);
		texcoords			*=2.0;
		vec2 texc_offset	=vec2(0.1,0.1)*amplitude*u.xy;
		texcoords			+=mult*texc_offset;
		dens				+=mult*(u.x*u.y);
		dens				*=saturate(1.0-.4*mult*length(texc_offset));
		mult				*=persistence;
		current_phase		*=2.0;
    }
    vec4 result;
	float a=0;//amplitude/240.0;
	result.a			=saturate((dens+2.0*density+diffusivity-2.0-a)/(1-a)/diffusivity);
	result.xyz			=result.aaa;//saturate(dens*1.5);
    return result;
}

#define DETAIL_STEPS 36
vec4 DetailLighting(Texture2D imageTexture,vec2 texcoords,vec2 detailTextureSize)
{
	float c=texture_wrap(imageTexture,texcoords).r;
	//Scale the offset by one texel.
	vec2 offset=0.5*lightDir2d.xy/detailTextureSize;
	float dens_dist=0.0;
    for(int i=0;i<DETAIL_STEPS;i++)
    {
		texcoords+=offset;
		float v=texture_wrap(imageTexture,texcoords).r;
		//v=saturate(v);//0.2*density-0.1);
		dens_dist+=v;
		if(v==0)
			dens_dist*=0.8;
    }
	float l=dens_dist/float(DETAIL_STEPS);
    return vec4(l,l,l,c);
}

#endif