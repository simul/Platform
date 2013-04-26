uniform_buffer GpuCloudConstants R2
{
	uniform mat4 transformMatrix;
	uniform mat4 vertexMatrix;
	uniform int octaves;
	uniform float persistence;
	uniform float humidity;
	uniform float time;
	uniform vec3 noiseScale;
	uniform float zPosition;
	uniform vec2 extinctions;
	uniform float zPixel;
	uniform float zSize;
	uniform float baseLayer;
	uniform float transition;
	uniform float upperDensity;
};

#ifndef __cplusplus
uniform sampler3D volumeNoiseTexture;

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
	const float default_=1.0;
	float i=saturate((z-baseLayer)/transition);
	float res1=default_*(1.0-i)+upperDensity*i;
	float res2=res1*saturate((1.0-z)/transition);
	return res2;
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
		float lookup=texture(volumeNoiseTexture,pos).x;
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
#endif