#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL

vec4 DetailDensity(vec2 texcoords,Texture2D imageTexture,float amplitude)
{
	float dens			=0.0;
	float mult			=0.5;
	float current_phase	=phase;
    for(int i=0;i<2;i++)//octaves
    {
		vec4 c			=texture_wrap(imageTexture,texcoords);
		vec2 u			=saturate(cos(2.0*3.14159*4*texcoords.xy)/(4.0+c.a)+c.a);//(c+current_phase));
		
		texcoords		*=2.0;
		texcoords		+=mult*vec2(0.1,0.1)*amplitude*u.xy;
		dens			+=mult*(u.x*u.y);
		mult			*=persistence;
		current_phase	*=2.0;
    }
    vec4 result;
	result.a			=saturate((dens+2.0*density+diffusivity-2.0)/diffusivity);
	result.xyz			=result.aaa;//saturate(dens*1.5);
    return result;
}

vec4 DetailLighting(vec2 texcoords,Texture2D imageTexture)
{
	vec4 c=texture_wrap(imageTexture,texcoords);
	//c.a=saturate(c.a-.5);
    return c;
	vec2 offset=lightDir2d.xy/256.0;
	float dens_dist=0.0;
    for(int i=0;i<12;i++)
    {
		texcoords+=offset;
		vec4 v=texture_wrap(imageTexture,texcoords);
		v.a=saturate(v.a+0.1);//0.2*density-0.1);
		dens_dist+=v.a;
		if(v.a==0)
			dens_dist*=0.9;
    }
    return vec4(dens_dist/12.0,dens_dist/12.0,dens_dist/12.0,c.a);
}

#endif