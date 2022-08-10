#include <math.h>
#include <stdlib.h>
#include "./Noise1D.h"
#include "Platform/Math/RandomNumberGenerator.h"
#include "Platform/Core/MemoryInterface.h"

using namespace platform::math;

static float cos_interp(float x, float a, float b)
{
	float ft = x * 3.1415927f;
	float f = (1.f - (float)cos(ft)) * .5f;
	return  a*(1.f-f) + b*f;
}

platform::math::Noise1D::Noise1D(platform::core::MemoryInterface *mem)
	:noise_buffer(NULL)
	,buffer_size(0)
	,memoryInterface(mem)
	,generation_number(0)
{
	frequency=0;
	filter=NULL;
	noise_random=new(memoryInterface) RandomNumberGenerator(memoryInterface);
	
}

platform::math::Noise1D::~Noise1D()
{
	operator delete[](noise_buffer,memoryInterface);
	del(noise_random,memoryInterface);
}

void platform::math::Noise1D::Setup(unsigned freq,int RandomSeed,int octaves,float p)
{
	persistence=p;
	numOctaves=octaves;
	noise_random->Seed(RandomSeed);
	frequency=freq;

	operator delete[](noise_buffer,memoryInterface);
	buffer_size=frequency;
	noise_buffer=new(memoryInterface) float[frequency];

	float *ptr=noise_buffer;
	for(unsigned i=0;i<frequency;i++)
	{
		*ptr =noise_random->FRand();
		ptr++;
	}
	generation_number++;
}

int platform::math::Noise1D::GetNoiseFrequency() const
{
	return frequency;
}

float platform::math::Noise1D::value_at(unsigned i) const
{
	i=i%frequency;
	return noise_buffer[i];
}

float platform::math::Noise1D::noise1(float p) const
{
	float x=p;
	int i1=((int)x);
	float sx=x-i1;
	int i2=(i1+1);

	float u,v,a;

	u=value_at(i1);		// left back
	v=value_at(i2);		// right front
	a = cos_interp(sx, u, v);		// back x=i1
	return a;
}

float platform::math::Noise1D::PerlinNoise1D(float x) const
{
	int i;
	float pos;
	float val,result = 0,sum=0;
	float scale = .5f;
	pos = x*frequency;
	for (i=0;i<numOctaves;i++)
	{
		val = noise1(pos);
		if(filter)
			val=filter->Filter(val);
		result += val * scale;
		sum+=scale;
		scale *= persistence;
		pos *= 2.f;		// double the position values
	}
	result/=sum;
	return(result);
}
void platform::math::Noise1D::SetFilter(NoiseFilter *nf)
{
	filter=nf;
}

unsigned platform::math::Noise1D::GetMemoryUsage() const
{
	return sizeof(this)+sizeof(RandomNumberGenerator)+buffer_size*sizeof(float);
}