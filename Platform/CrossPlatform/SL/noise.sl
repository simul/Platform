//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef PLATFORM_CROSSPLATFORM_NOISE_SL
#define PLATFORM_CROSSPLATFORM_NOISE_SL



float rand(float c)
{
    return fract(sin(dot(vec2(c,11.1*c) ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand3(vec3 co)
{
    return fract(sin(dot(co.xyz,vec3(12.9898,78.233,42.1897))) * 43758.5453);
}
/*
float randhash(unsigned seed,float b)
{
	float InverseMaxInt=1.0/4294967295.0;
	unsigned i=(seed^unsigned(12345391))*unsigned(2654435769);
	i^=(i<<unsigned(6))^(i>>unsigned(26));
	i*=unsigned(2654435769);
	i+=(i<<unsigned(5))^(i>>unsigned(12));
	return float(b*i)*InverseMaxInt;
}*/

vec3 SphericalRandom(vec3 co)
{
	float r			=1.f-pow(rand3(co),4.0);
	float az		=rand3(43.1138*co)*2*3.1415926536;
	float sine_el	=rand3(17.981*co)*2.0-1.0;
	float el		=asin(sine_el);
	float cos_el	=cos(el);
	vec3 v;
	v.x				=r*sin(az)*cos_el;
	v.y				=r*cos(az)*cos_el;
	v.z				=r*sine_el;
	return v;
}

vec4 Noise(Texture2D noise_texture,vec2 texCoords,float persistence,int octaves)
{
	vec4 result		=vec4(0,0,0,0);
	float mult		=0.5;
	float total		=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c		=texture_wrap_lod(noise_texture,texCoords,0);
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*c;
		mult		*=persistence;
    }
	// divide by total to get the range -1,1.
	result*=1.0/total;
	//result.xyz=texCoords.xyy;
    return result;
}

vec4 VirtualNoiseLookup(vec3 texCoords,int gridsize,int seed)
{
	vec4 result		=vec4(0,0,0,0);
	vec3 pos		=texCoords*gridsize;
	vec3 intpart,floatpart;
	floatpart		=modf(pos,intpart);
	int3 seedpos	=int3(1*seed,2*seed,3*seed);
	int3 firstCorner=int3(intpart);
    for (int i = 0; i < 2; i++)
	{
        for (int j  = 0; j < 2; j++)
		{
            for (int k  = 0; k < 2; k++)
			{
				int3 corner_pos		=firstCorner+int3(1-i,1-j,1-k);
				// NOTE: operator % does NOT seem to work properly here.
				if(corner_pos.x==gridsize)
					corner_pos.x=0;
				if(corner_pos.y==gridsize)
					corner_pos.y=0;
				if(corner_pos.z==gridsize)
					corner_pos.z=0;
				vec3 lookup_pos		=seedpos+vec3(corner_pos);
				vec4 rnd_lookup		=vec4(SphericalRandom(lookup_pos),rand3(lookup_pos));
				float proportion	=abs(i-floatpart.x)*abs(j-floatpart.y)*abs(k-floatpart.z);
				result				+=rnd_lookup*proportion;
			}
		}
	}
	return result;
}


vec4 Noise3D(Texture3D random_texture_3d,int freq,vec3 texCoords,int octaves,float persistence,float strength)
{
	vec4 result		=vec4(0,0,0,0);
	float mult		=0.5;
	float total		=0.0;
	float prevx		=1.0;
	vec3 last;
	{
		vec4 c		=texture_3d_wrap_lod(random_texture_3d,texCoords,0);
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*cos(2.0*3.1415926536*prevx)*c*1.414;
		mult		*=persistence;
		prevx		=c.a;
		last		=c.rgb;
	}
    for(int i=1;i<octaves;i++)
    {
		vec4 c		=texture_3d_wrap_lod(random_texture_3d,texCoords,0);
		vec3 u		=cross(last.rgb,c.rgb);
		float ul	=length(u)+0.001;
		u			=(u)/ul*length(c.rgb);
		c.rgb		=u;
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*cos(2.0*3.1415926536*prevx)*c*1.414;
		mult		*=persistence;
		prevx		=c.a;
		last		=c.rgb;
    }
	// divide by total to get the range -1,1.
	result		*=strength/total;
	result		=clamp(result,vec4(-1.0,-1.0,-1.0,-1.0),vec4(1.0,1.0,1.0,1.0));
	return result;
}

vec4 permute(vec4 x)
{
	return fmod((34.0*x+1.0)*x,289);
}

float cellular3x3(vec3 P)
{
	const float K		=1.0/7.0;
	const float K2		=0.5/7.0;
	const float jitter	=0.8;
	vec3 Pi		=fmod(floor(P),289.0);
	vec3 Pf		=fract(P);
	vec4 Pfx	=Pf.x+vec4(-0.5,-1.5,-0.5,-1.5);
	vec4 Pfy	=Pf.y+vec4(-0.5,-0.5,-1.5,-1.5);
	vec4 Pfz	=Pf.z+vec4(-0.5,-0.5,-0.5,-1.5);

	vec4 p 		=permute(Pi.x+vec4(0.0,1.0,0.0,1.0));
	p 			=permute(p+Pi.y+vec4(0.0,0.0,1.0,1.0));
	p 			=permute(p+Pi.z+vec4(0.0,0.0,1.0,1.0));

	vec4 ox		=fmod(p,7.0)*K+K2;
	vec4 oy		=fmod(floor(p*K),7.0)*K+K2;
	vec4 oz		=fmod(floor(p*K),7.0)*K+K2;

	vec4 dx		=Pfx+jitter*ox;
	vec4 dy		=Pfy+jitter*oy;
	vec4 dz		=Pfz+jitter*oz;

	vec4 d=dx*dx+dy*dy+dz*dz;		// dist sq

	d.x			=min(min(min(d.x,d.y),d.z),d.w);
	return 1.0-1.5*d.x;
}
#endif