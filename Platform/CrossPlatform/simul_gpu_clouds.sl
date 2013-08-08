#ifndef SIMUL_GPU_CLOUDS_SL
#define SIMUL_GPU_CLOUDS_SL

uniform_buffer GpuCloudConstants R8
{
	uniform mat4 transformMatrix;
	uniform vec4 yRange;
	uniform int octaves;
	uniform int a,b,c;
	uniform float persistence;
	uniform float d,e,f;
	uniform float humidity;
	uniform float g,h,k;
	uniform float time;
	uniform float l,m,n;
	uniform vec3 noiseScale;
	uniform float o;
	uniform float zPosition;
	uniform float p,q,r;
	uniform vec2 extinctions;
	uniform float s,ttt;
	uniform float zPixel;
	uniform float u,v,w;
	uniform float zSize;
	uniform float aa,bb,cc;
	uniform float baseLayer;
	uniform float dd,ee,ff;
	uniform float transition;
	uniform float gg,hh,ii;
	uniform float upperDensity;
	uniform float jj,kk,ll;
	uniform float diffusivity;
	uniform float mm,nn,oo;
	uniform float invFractalSum;
	uniform float pp,qq,rr;
};

#ifndef __cplusplus

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

float NoiseFunction(Texture3D volumeNoiseTexture,vec3 pos,int octaves,float persistence,float time)
{
	float dens=0.0;
	float mult=0.5;
	float sum=0.0;
	float t=time;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		float lookup=texture_wrap(volumeNoiseTexture,pos).x;
		float val=lookup;
		dens+=mult*val;
		sum+=mult;
		mult*=persistence;
		pos*=2.0;
		t*=2.0;
	}
	dens/=sum;
	return dens;
}
#endif
#endif
