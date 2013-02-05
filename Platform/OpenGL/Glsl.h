#pragma once
#ifdef __cplusplus
typedef int sampler1D;
typedef int sampler2D;
typedef int sampler3D;
#define layout(std140) struct
#define uniform
#define ALIGN
#define cbuffer layout(std140) uniform
#define R0

struct vec2
{
	float x,y;
	vec2(float x=0,float y=0)
	{
		this->x=x;
		this->y=y;
	}
	operator const float *()
	{
		return &x;
	}
	void operator=(const float *v)
	{
		x=v[0];
		y=v[1];
	}
};

struct vec3
{
	float x,y,z;
	vec3(float x=0,float y=0,float z=0)
	{
		this->x=x;
		this->y=y;
		this->z=z;
	}
	operator const float *()
	{
		return &x;
	}
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
	vec4(float x=0,float y=0,float z=0,float w=0)
	{
		this->x=x;
		this->y=y;
		this->z=z;
		this->w=w;
	}
	operator const float *()
	{
		return &x;
	}
	void operator=(const float *v)
	{
		x=v[0];
		y=v[1];
		z=v[2];
		w=v[3];
	}
};
#endif