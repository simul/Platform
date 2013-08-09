#ifndef CPPSL_HS
#define CPPSL_HS

// Definitions shared across C++, HLSL, and GLSL!

#ifndef __cplusplus
	#define ALIGN_16
#endif

#ifdef __cplusplus
#ifdef _MSC_VER
	#pragma warning(disable:4324)
	#define ALIGN_16 __declspec( align( 16 ) )
#else
	#define ALIGN_16
#endif
	#define STATIC static
	#define uniform
	#define cbuffer struct

	#define SIMUL_BUFFER_REGISTER(buff_num)
	#define SIMUL_SAMPLER_REGISTER(buff_num)
	#define SIMUL_BUFFER_REGISTER(buff_num)

	#define R0
	#define R1
	#define R2
	#define R3
	#define R4
	#define R5
	#define R6
	#define R7
	#define R8
	#define R9
	#define R10
	#define R13

	#define uniform_buffer ALIGN_16 cbuffer

	struct mat2
	{
		float m[4];
		operator const float *()
		{
			return m;
		}
		void operator=(const float *v)
		{
			for(int i=0;i<4;i++)
				m[i]=v[i];
		}
		void transpose()
		{
			for(int i=0;i<2;i++)
				for(int j=0;j<2;j++)
					if(i<j)
					{
						float temp=m[i*2+j];
						m[i*2+j]=m[j*2+i];
						m[j*2+i]=temp;
					}
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
		void transpose()
		{
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					if(i<j)
					{
						float temp=m[i*4+j];
						m[i*4+j]=m[j*4+i];
						m[j*4+i]=temp;
					}
		}
	};

	struct vec2
	{
		float x,y;
		vec2(float X=0.f,float Y=0.f)
			:x(X),y(Y)
		{
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
		vec3(const float *v)
		{
			operator=(v);
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

	struct uint3
	{
		unsigned x,y,z;
		uint3(unsigned x=0,unsigned y=0,unsigned z=0)
		{
			this->x=x;
			this->y=y;
			this->z=z;
		}
		uint3(const int *v)
		{
			operator=(v);
		}
		uint3(const unsigned *v)
		{
			operator=(v);
		}
		operator const unsigned *()
		{
			return &x;
		}
		void operator=(const int *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
		}
		void operator=(const unsigned *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
		}
	};
#endif

#endif