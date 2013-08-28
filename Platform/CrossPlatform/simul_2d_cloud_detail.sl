#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL

vec4 DetailDensity(vec2 texcoords,Texture2D imageTexture)
{
	vec4 result=vec4(0,0,0,0);
	float mult=0.5;
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture_wrap(imageTexture,texcoords);
		texcoords*=2.0;
		texcoords+=mult*vec2(0.2,0.2)*c.xy;
		result+=mult*c;
		mult*=persistence;
    }
    result.rgb=saturate(result.rrr*1.5);
    return result;
}

vec4 DetailLighting(vec2 texcoords,Texture2D imageTexture)
{
	vec4 c=texture_wrap(imageTexture,texcoords);
	vec2 offset=lightDir2d.xy/256.0;
	float dens_dist=0.0;
    for(int i=0;i<16;i++)
    {
		texcoords+=offset;
		vec4 v=texture_wrap(imageTexture,texcoords);
		v.a=saturate(v.a+0.2*cloudiness-0.1);
		dens_dist+=v.a;
		if(v.a==0)
			dens_dist*=0.9;
    }
    return vec4(dens_dist,dens_dist,dens_dist,c.a);
}

#endif