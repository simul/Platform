#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL

vec4 DetailDensity(vec2 texcoords,Texture2D imageTexture,float amplitude)
{
	vec4 result=vec4(0,0,0,0);
	float mult=0.5;
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture_wrap(imageTexture,texcoords);
		texcoords*=2.0;
		texcoords+=mult*vec2(0.1,0.1)*amplitude*c.xy;
		result+=mult*c;
		mult*=persistence;
    }
    result.rgb=saturate(result.rrr*1.5);
	result.a=saturate(result.a+1.2*density-0.5);//(result.a+density+diffusivity-1.0)/diffusivity);
    return result;
}

vec4 DetailLighting(vec2 texcoords,Texture2D imageTexture)
{
	vec4 c=texture_wrap(imageTexture,texcoords);
	c.a=saturate(c.a-.5);
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