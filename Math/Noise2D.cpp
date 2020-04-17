#include <math.h>
#include <stdlib.h>
#include "./Noise2D.h"
#include "Platform/Math/RandomNumberGenerator.h"
#include "Platform/Core/MemoryInterface.h"

using namespace simul::math;

static float cos_interp(float x, float a, float b)
{
	float ft = x * 3.1415927f;
	float f = (1.f - cosf(ft)) * .5f;
	return  a*(1.f-f) + b*f;
}

simul::math::Noise2D::Noise2D(simul::base::MemoryInterface *mem):buffer_size(0)
	,generation_number(0)
{
	memoryInterface=mem;
	frequency=0;
	noise_buffer=NULL;
	filter=NULL;
	noise_random=new(memoryInterface) RandomNumberGenerator(memoryInterface);

}

simul::math::Noise2D::~Noise2D()
{
	operator delete[](noise_buffer,memoryInterface);
	del(noise_random,memoryInterface);
}

void simul::math::Noise2D::Setup(unsigned freq,int RandomSeed,int octaves,float p)
{
	persistence=p;
	numOctaves=octaves;
	noise_random->Seed(RandomSeed);
	frequency=freq;
	
	operator delete[](noise_buffer,memoryInterface);
	buffer_size=frequency*frequency;
	noise_buffer=new(memoryInterface) float[buffer_size];
	
	float *ptr=noise_buffer;
	for(unsigned i=0;i<frequency*frequency;i++)
	{
		*ptr =noise_random->FRand();
		ptr++;
	}
	generation_number++;
}

int simul::math::Noise2D::GetNoiseFrequency() const
{
	return frequency;
}


float simul::math::Noise2D::value_at(unsigned i,unsigned j) const
{
	i=i%frequency;
	j=j%frequency;
	return noise_buffer[j*frequency+i];
}

float simul::math::Noise2D::noise3(int p[2]) const
{
	int x=p[0];
	int y=p[1];
	int i1=((int)x);
	int j1=((int)y);

	return value_at(i1,j1);
}

float simul::math::Noise2D::noise3(float p[2]) const
{
	float x=p[0];
	float y=p[1];
	int i1=((int)x);
	int j1=((int)y);
	float sx=x-i1;
	float sy=y-j1;
	int i2=(i1+1);
	int j2=(j1+1);

	float u,v,a,b;

	u=value_at(i1,j1);				// left back
	v=value_at(i2,j1);				// right front
	a = cos_interp(sx, u, v);		// back x=i1

	u=value_at(i1,j2);				// left bottom back
	v=value_at(i2,j2);				// right bottom front
	b = cos_interp(sx, u, v);		// front x=i2

	return cos_interp(sy, a, b);
}

float simul::math::Noise2D::PerlinNoise2D(float x,float y) const
{
	float pos[2];
	float val,result = 0,sum=0;
	float scale = .5f;
	pos[0]=x*frequency;
	pos[1]=y*frequency;
	for(int i=0;i<numOctaves;i++)
	{
		val=noise3(pos);
		if(filter)
			val=filter->Filter(val);
		result+=val*scale;
		sum+=scale;
		scale*=persistence;
		pos[0]*=2.f;		// double the position values
		pos[1]*=2.f;		// this is like making the noise grid 1/2 the size.
	}
	result/=sum;
	return(result);
}
// Because we input ints, it is necessary to start with the high-frequency octaves
// and go to the lower ones.
float simul::math::Noise2D::PerlinNoise2D(int x,int y) const
{
	int i;
	float val,result = 0,sum=0;
	int pos[2];
	// after each loop scale gets bigger.
	// we want the final scale to be 0.5
	// this scale is reached after (numOctaves-1) loop steps
	float scale = (float)pow(persistence,(float)numOctaves);
	pos[0] = x;
	pos[1] = y;
	for (i=0;i<numOctaves;i++)
	{
		val = noise3(pos);
		if(filter)
			val=filter->Filter(val);
		result += val * scale;
		sum+=scale;
		scale /= persistence;
		pos[0] /= 2;		// half the position values
		pos[1] /= 2;		// this is like making the noise grid double the size.
	}
	result/=sum;
	return(result);
}

void simul::math::Noise2D::SetFilter(NoiseFilter *nf)
{
	filter=nf;
}

unsigned simul::math::Noise2D::GetMemoryUsage() const
{
	return sizeof(this)+sizeof(RandomNumberGenerator)+buffer_size*sizeof(float);
}