#ifndef SIMUL_GPU_CLOUDS_SL
#define SIMUL_GPU_CLOUDS_SL

uniform_buffer GpuCloudConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 transformMatrix;
	uniform vec4 yRange;
	uniform uint3 threadOffset;
	uniform float ss;
	uniform vec2 extinctions;
	uniform float stepLength,ttt;
	uniform uint3 gaussianOffset;
	uniform int octaves;
	uniform vec3 noiseScale;
	uniform float zPosition;

	uniform float time;
	uniform float persistence;
	uniform float humidity;
	uniform float zPixelLightspace;

	uniform float zPixel;
	uniform float zSize;
	uniform float baseLayer;
	uniform float transition;

	uniform float upperDensity;
	uniform float diffusivity;
	uniform float invFractalSum;
	uniform float maskThickness;

	uniform vec2 maskCentre;
	uniform float maskRadius;
	uniform float maskFeather;
};

#ifndef __cplusplus

uint3 LinearThreadToPos3D(uint linear_pos,uint3 dims)
{
	uint Z				=linear_pos/dims.x/dims.y;
	uint Y				=(linear_pos-Z*dims.x*dims.y)/dims.x;
	uint X				=linear_pos-Y*(dims.x+Z*dims.y);
	uint3 pos			=uint3(X,Y,Z);
	return pos;
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
	const float default_=1.0;
	float i=saturate((z-baseLayer)/transition);
	float res1=default_*(1.0-i)+upperDensity*i;
	float res2=res1*lerp(0.75,1.0,saturate((1.0-z)/transition));
	return res2;
}

float NoiseFunction(Texture3D volumeNoiseTexture,vec3 pos,int octaves,float persistence,float t)
{
	float dens=0.0;
	float mult=0.5;
	float sum=0.0;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		float lookup=texture_wrap_lod(volumeNoiseTexture,pos,0).x;
		float val	=lookup;//0.5*(1.0+cos(2.0*3.1415926536*(lookup+t)));
		dens		=dens+mult*val;
		sum			=sum+mult;
		mult		=mult*persistence;
		pos			=pos*2.0;
		t			=t*2.0;
	}
	dens=dens/sum;
	return dens;
}
#endif
#endif
