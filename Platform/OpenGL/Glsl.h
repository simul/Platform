#pragma once

#define layout(std140) struct
#define uniform

struct vec2
{
	float x,y;
	void operator=(const float *v)
	{
		x=v[0];
		y=v[1];
	}
};

struct vec3
{
	float x,y,z;
	void operator=(const float *v)
	{
		x=v[0];
		y=v[1];
		z=v[2];
	}
};

struct vec4
{
	float x,y,z,w;
	void operator=(const float *v)
	{
		x=v[0];
		y=v[1];
		z=v[2];
		w=v[3];
	}
};