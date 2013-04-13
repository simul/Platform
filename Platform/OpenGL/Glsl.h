#ifndef GLSL_H
#define GLSL_H
// These definitions translate the HLSL terms ALIGN, cbuffer and R0 for GLSL or C++
#define ALIGN
#define uniform_buffer layout(std140) uniform
#define R0
#define R1
#define R2

#ifdef __cplusplus

// To C++, samplers are just GLint's.
typedef int sampler1D;
typedef int sampler2D;
typedef int sampler3D;

// C++ sees a layout as a struct, and doesn't care about uniforms
#define layout(std140) struct
#define uniform

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

struct mat4
{
	float m[16];
	operator const float *()
	{
		return m;
	}
	void operator=(const float *v)
	{
		for(int i=0;i<16;i++)
			m[i]=v[i];
	}
};

#endif
#endif