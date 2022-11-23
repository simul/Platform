//  Copyright (c) 2015-17 Simul Software Ltd. All rights reserved.
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

vec3 rand3v( vec3 p ) 
{
	return fract(sin(vec3(dot(p,vec3(127.1,311.7,55.3221)),
			  			  dot(p,vec3(269.5,183.3,251.1732)),
			  			  dot(p,vec3(12.9898,42.1287,78.233))))
                 			* 43758.5453123);
}


float randhash(uint seed,float b)
{
	float InverseMaxInt=1.0/4294967295.0;
	uint i=(seed^uint(12345391))*uint(2654435769);
	i^=(i<<uint(6))^(i>>uint(26));
	i*=uint(2654435769);
	i+=(i<<uint(5))^(i>>uint(12));
	return float(b*i)*InverseMaxInt;
}

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

vec4 VirtualNoiseLookup(vec3 texCoords,int gridsize,int seed,bool pfilter)
{
	vec4 result		=vec4(0,0,0,0);
	vec3 pos		=frac(texCoords)*gridsize;
	vec3 intpart,floatpart;
	floatpart		=modf(pos,intpart);
	int3 seedpos	=int3(2*seed,17*seed,7*seed);
	int3 firstCorner=int3(intpart);
	if (pfilter)
	{
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < 2; j++)
			{
				for (int k = 0; k < 2; k++)
				{
					int3 corner_pos = firstCorner + int3(1 - i, 1 - j, 1 - k);
					// NOTE: operator % does NOT seem to work properly here.
					if (corner_pos.x == gridsize)
						corner_pos.x = 0;
					if (corner_pos.y == gridsize)
						corner_pos.y = 0;
					if (corner_pos.z == gridsize)
						corner_pos.z = 0;
					vec3 lookup_pos		=seedpos + vec3(corner_pos);
                    float rndTap        =rand3(lookup_pos);
					vec4 rnd_lookup		=vec4(rndTap,rndTap,rndTap,rndTap);
					float proportion	=abs(i - floatpart.x)*abs(j - floatpart.y)*abs(k - floatpart.z);
					result				+=rnd_lookup*proportion;
				}
			}
		}
	}
	else
	{
		// nearest.
		int3 corner_pos = int3(pos+vec3(0.5, 0.5, 0.5));
		vec3 lookup_pos = seedpos + vec3(corner_pos);
        float rndTap = rand3(lookup_pos);
		result       = vec4(rndTap,rndTap,rndTap,rndTap);
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
		u			=u/ul*length(c.rgb);
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

vec4 permute4(vec4 x)
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
	vec4 Pfx	=Pf.xxxx+vec4(-0.5,-1.5,-0.5,-1.5);
	vec4 Pfy	=Pf.yyyy+vec4(-0.5,-0.5,-1.5,-1.5);
	vec4 Pfz	=Pf.zzzz+vec4(-0.5,-0.5,-0.5,-1.5);

	vec4 p 		=permute4(Pi.xxxx+vec4(0.0,1.0,0.0,1.0));
	p 			=permute4(p+Pi.yyyy+vec4(0.0,0.0,1.0,1.0));
	p 			=permute4(p+Pi.zzzz+vec4(0.0,0.0,1.0,1.0));

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


//
// Description : Array and textureless GLSL 2D/3D/4D simplex 
//               noise functions.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
// 

vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute4a(vec4 x) {
     return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v)
  { 
  const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

// First corner
  vec3 i  = floor(v + dot(v, C.yyy) );
  vec3 x0 =   v - i + dot(i, C.xxx) ;

// Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min( g.xyz, l.zxy );
  vec3 i2 = max( g.xyz, l.zxy );

  //   x0 = x0 - 0.0 + 0.0 * C.xxx;
  //   x1 = x0 - i1  + 1.0 * C.xxx;
  //   x2 = x0 - i2  + 2.0 * C.xxx;
  //   x3 = x0 - 1.0 + 3.0 * C.xxx;
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
  vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

// Permutations
  i = mod289(i); 
  vec4 p = permute4a( permute4a( permute4a( 
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 )) 
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

// Gradients: 7x7 points over a square, mapped onto an octahedron.
// The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
  float n_ = 0.142857142857; // 1.0/7.0
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

  //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
  //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0, 0, 0, 0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);

//Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;

// Mix final noise value
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), 
                                dot(p2,x2), dot(p3,x3) ) );
  }
  

// from http://http.developer.nvidia.com/GPUGems3/gpugems3_ch37.html
struct RandomResult
{
    uint4 state;
    float value;
};

uint TausStep(uint z, int S1, int S2, int S3, uint M)
{
    uint b = (((z << S1) ^ z) >> S2);
    return (((z & M) << S3) ^ b);    
}

uint LCGStep(uint z, uint A, uint C)
{
    return (A * z + C);    
}

void CombinedTauswortheRandom(inout RandomResult result)
{
    result.state.x = TausStep(result.state.x, 13, 19, 12, 4294967294);
    result.state.y = TausStep(result.state.y, 2, 25, 4, 4294967288);
    result.state.z = TausStep(result.state.z, 3, 11, 17, 4294967280);
    result.state.w = LCGStep(result.state.w, 1664525, 1013904223);

    result.value = 2.3283064365387e-10 * (result.state.x ^ result.state.y ^ result.state.z ^ result.state.w);

}

vec3 CombinedTauswortheSphericalRandom(inout RandomResult result)
{
	CombinedTauswortheRandom(result);
	float r			=1.f-pow(result.value,4.0);
	CombinedTauswortheRandom(result);
	float az		=result.value*2*3.1415926536;
	CombinedTauswortheRandom(result);
	float sine_el	=result.value*2.0-1.0;
	float el		=asin(sine_el);
	float cos_el	=cos(el);
	vec3 v;
	v.x				=r*sin(az)*cos_el;
	v.y				=r*cos(az)*cos_el;
	v.z				=r*sine_el;
	return v;
}

vec4 TauswortheVirtualNoiseLookup(vec3 texCoords,int gridsize,int seed,bool pfilter)
{
	vec4 result		=vec4(0,0,0,0);
	vec3 pos		=frac(texCoords)*gridsize;
	vec3 intpart,floatpart;
	floatpart		=modf(pos,intpart);
	int3 seedpos	=int3(2*seed,17*seed,7*seed);
	int3 firstCorner=int3(intpart);
	RandomResult randomResult;
	randomResult.state=uint4(seed+128+intpart.x,seed+23465+intpart.y,seed+2174+intpart.z,seed+1902847+intpart.x);
	if (pfilter)
	{
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < 2; j++)
			{
				for (int k = 0; k < 2; k++)
				{
					int3 corner_pos = firstCorner + int3(1 - i, 1 - j, 1 - k);
					// NOTE: operator % does NOT seem to work properly here.
					if (corner_pos.x == gridsize)
						corner_pos.x = 0;
					if (corner_pos.y == gridsize)
						corner_pos.y = 0;
					if (corner_pos.z == gridsize)
						corner_pos.z = 0;
					vec3 lookup_pos		=seedpos + vec3(corner_pos);
                    
					CombinedTauswortheRandom(randomResult);
					vec4 rnd_lookup		=vec4(randomResult.value,randomResult.value,randomResult.value,randomResult.value);
					float proportion	=abs(i - floatpart.x)*abs(j - floatpart.y)*abs(k - floatpart.z);
					result				+=rnd_lookup*proportion;
				}
			}
		}
	}
	else
	{
		// nearest.
		int3 corner_pos = int3(pos+vec3(0.5, 0.5, 0.5));
		vec3 lookup_pos = seedpos + vec3(corner_pos);
        float rndTap = rand3(lookup_pos);
		result       = vec4(rndTap,rndTap,rndTap,rndTap);
	}
	return result;
}

#endif