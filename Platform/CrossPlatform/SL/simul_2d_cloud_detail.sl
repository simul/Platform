#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL

vec4 DetailDensity(vec2 texcoords,Texture2D imageTexture,float amplitude)
{
	float dens			=0.0;
	float mult			=0.5;
	float current_phase	=phase;
    for(int i=0;i<octaves;i++)//
    {
		vec4 c			=texture_wrap(imageTexture,texcoords);
		vec2 u			=saturate(cos(2.0*3.14159*texcoords.xy)/(1.515+7.135*c.a)+c.a);//(c+current_phase));
		
		texcoords		*=2.0;
		texcoords		+=mult*vec2(0.1,0.1)*amplitude*u.xy;
		dens			+=mult*(u.x*u.y);
		mult			*=persistence;
		current_phase	*=2.0;
    }
    vec4 result;
	float a=amplitude/40.0;
	result.a			=saturate((dens+2.0*density+diffusivity-2.0-a)/(1-a)/diffusivity);
	result.xyz			=result.aaa;//saturate(dens*1.5);
    return result;
}

vec4 DetailLighting(vec2 texcoords,Texture2D imageTexture)
{
	float c=texture_wrap(imageTexture,texcoords).r;
	//Scale the offset by one texel.
	vec2 offset=.5*lightDir2d.xy/detailTextureSize;
	float dens_dist=0.0;
    for(int i=0;i<12;i++)
    {
		texcoords+=offset;
		float v=texture_wrap(imageTexture,texcoords).r;
		//v=saturate(v);//0.2*density-0.1);
		dens_dist+=v;
		if(v==0)
			dens_dist*=0.8;
    }
    return vec4(dens_dist/12.0,dens_dist/12.0,dens_dist/12.0,c);
}

#endif