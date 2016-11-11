//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_2D_CLOUD_DETAIL_SL
#define SIMUL_2D_CLOUD_DETAIL_SL


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