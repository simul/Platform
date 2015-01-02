#ifndef PLATFORM_CROSSPLATFORM_NOISE_SL
#define PLATFORM_CROSSPLATFORM_NOISE_SL

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand3(vec3 co)
{
    return fract(sin(dot(co.xyz,vec3(12.9898,78.233,42.1897))) * 43758.5453);
}

vec3 SphericalRandom(vec3 co)
{
	float r=1.f-pow(rand3(co),4.0);
	float az=rand3(43.1138*co)*2*3.1415926536;
	float el=asin(rand3(17.981)*2.0-1.0);
	vec3 v;
	v.x=r*sin(az)*cos(el);
	v.y=r*cos(az)*cos(el);
	v.z=r*sin(el);
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
    return result;
}

vec4 Noise3D(Texture3D random_texture_3d,vec3 texCoords,int octaves,float persistence)	//SV_DispatchThreadID gives the combined id in each dimension.
{
	vec4 result		=vec4(0,0,0,0);
	float mult		=0.5;
	float total		=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c		=texture_wrap_lod(random_texture_3d,texCoords,0);
		texCoords	*=2.0;
		total		+=mult;
		result		+=mult*c;
		mult		*=persistence;
    }
	// divide by total to get the range -1,1.
	result			*=1.0/total;
	//result.a		=0.5*(result.a+1.0);
	return result;
}
#endif