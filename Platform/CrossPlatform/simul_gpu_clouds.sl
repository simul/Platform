uniform float zPixel;
uniform float zSize;
uniform sampler3D volumeNoiseTexture;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

vec2 saturate(vec2 v)
{
	return clamp(v,vec2(0.0,0.0),vec2(1.0,1.0));
}

vec3 assemble3dTexcoord(vec2 texcoord2)
{
	vec2 texcoord	=texcoord2.xy;
	texcoord.y		*=zSize;
	float zPos		=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y		=fract(texcoord.y);
	vec3 texcoord3	=vec3(texcoord.xy,zPos);
	return texcoord3;
}

float GetHumidityMultiplier(float z)
{
	const float base_layer=0.46;
	const float transition=0.2;
	const float mul=0.7;
	const float default_=1.0;
	float i=saturate((z-base_layer)/transition);
	return default_*(1.0-i)+mul*i;
}

float NoiseFunction(vec3 pos,int octaves,float persistence,float time)
{
	float dens=0.0;
	float mul=0.5;
	float this_grid_height=1.0;
	float sum=0.0;
	float t=time;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		float lookup=texture3D(volumeNoiseTexture,pos).x;
		float val=lookup;
		dens+=mul*val;
		sum+=mul;
		mul*=persistence;
		pos*=2.0;
		this_grid_height*=2.0;
		t*=2.0;
	}
	dens/=sum;
	return dens;
}