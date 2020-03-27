#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include "Platform/Math/Noise3D.h"
#include "Platform/Math/RandomNumberGenerator.h"
#include "Platform/Core/MemoryInterface.h"

using namespace simul::math;

static float lerp(float t, float a, float b)
{
	return ( a + t * (b - a) );
}
NoiseInterface::NoiseInterface(){}
NoiseInterface::~NoiseInterface(){}

simul::math::Noise3D::Noise3D(simul::base::MemoryInterface *mem)
	:generation_number(0)
	,buffer_size(0)
	,grid_x(0)
	,grid_y(0)
	,grid_z(0)
{
	memoryInterface=mem;
	frequency=0;
	noise_buffer=NULL;
	filter=NULL;
	noise_random=new(memoryInterface) RandomNumberGenerator(memoryInterface);
}

simul::math::Noise3D::~Noise3D()
{
	operator delete[](noise_buffer,memoryInterface);
	del(noise_random,memoryInterface);
}

void simul::math::Noise3D::Setup(unsigned freq,int RandomSeed,int octaves,float pers)
{
	persistence=pers;
	numOctaves=octaves;
	if(frequency!=freq||noise_random->GetSeed()!=RandomSeed)
	{
		frequency=freq;
		noise_random->Seed(RandomSeed);
		if(frequency*frequency*frequency>buffer_size)
		{
			buffer_size=frequency*frequency*frequency;
			operator delete[](noise_buffer,memoryInterface);
			noise_buffer=new(memoryInterface) float[buffer_size];
			
		}
		generation_number++;
		float *ptr=noise_buffer;

		for(unsigned z=0;z<freq;z++)
		{
			for(unsigned y=0;y<freq;y++)
			{
				for(unsigned x=0;x<freq;x++)
				{
					*ptr =noise_random->FRand();
					ptr++;
				}
			}
		}
	}
	sum=0;
	float effect=.5f;
	for(int i=0;i<numOctaves;i++)
	{
		sum+=effect;
		effect *= persistence;
	}
}

unsigned simul::math::Noise3D::GetNoiseFrequency() const
{
	return frequency;
}

float simul::math::Noise3D::value_at(unsigned i,unsigned j,unsigned k) const
{
	i=i%frequency;
	j=j%frequency;
	k=k%frequency;
	return noise_buffer[(k*frequency+j)*frequency+i];
}

float simul::math::Noise3D::noise3(int pos[3]) const
{
	int x=pos[0];
	int y=pos[1];
	int z=pos[2];
	int i1=((int)x);
	int j1=((int)y);
	int k1=((int)z);

	return value_at(i1,j1,k1);
}

float simul::math::Noise3D::noise3(float pos[3]) const
{
	float x=pos[0];
	float y=pos[1];
	float z=pos[2];
	int i1=((int)x);
	int j1=((int)y);
	int k1=((int)z);
	float sx=x-i1;
	float sy=y-j1;
	float sz=z-k1;
	int i2=(i1+1);
	int j2=(j1+1);
	int k2=(k1+1);

	float u,v,a,b,c,d;

	u=value_at(i1,j1,k1);		// left bottom back
	v=value_at(i2,j1,k1);		// right bottom front
	a = lerp(sx, u, v);			// bottom back x=i1

	u=value_at(i1,j2,k1);		// left bottom back
	v=value_at(i2,j2,k1);		// right bottom front
	b = lerp(sx, u, v);			// bottom front x=i2

	c = lerp(sy, a, b);			// bottom, z=k1

	u=value_at(i1,j1,k2);		// left top back
	v=value_at(i2,j1,k2);		// right top front
	a = lerp(sx, u, v);			// top back x=i1

	u=value_at(i1,j2,k2);		// left top back
	v=value_at(i2,j2,k2);		// right top front
	b = lerp(sx, u, v);			// top front y=j

	d = lerp(sy, a, b);			// top, z=k2

	return lerp(sz,c,d);
}

float simul::math::Noise3D::PerlinNoise3D(float x,float y,float z) const
{
	int i;
	float dens=0.f;
	float mult=.5f;
	float pos[3];
	pos[0]=x*frequency;
	pos[1]=y*frequency;
	pos[2]=z*frequency;
	float total=0.0;
	for(i=0;i<numOctaves;i++)
	{
		float lookup=noise3(pos);
		//if(filter)
		//	val=filter->Filter(val);
		dens+=lookup*mult;
		total+=mult;
		mult*=persistence;
		pos[0]*=2.f;		// double the position values
		pos[1]*=2.f;		// this is like making the noise grid 1/2 the size.
		pos[2]*=2.f;
	}
	dens/=total;
	return dens;
}

float simul::math::Noise3D::PerlinNoise3D(int x,int y,int z) const
{
	return PerlinNoise3D(((float)x+0.5f)/(float)grid_x,((float)y+0.5f)/(float)grid_y,((float)z+0.5f)/(float)grid_z);
}

void simul::math::Noise3D::SetFilter(NoiseFilter *nf)
{
	filter=nf;
}

unsigned simul::math::Noise3D::GetMemoryUsage() const
{
	unsigned mem=sizeof(this)+sizeof(RandomNumberGenerator)+buffer_size*sizeof(float);
	return mem;
}

void simul::math::Noise3D::SetCacheGrid(int x,int y,int z)
{
	grid_x=x;
	grid_y=y;
	grid_z=z;
	grid_max=std::max(std::max(grid_x,grid_y),grid_z);
}
