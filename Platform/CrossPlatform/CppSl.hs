#ifndef CPPSL_HS
#define CPPSL_HS
#undef RADIAL_CLOUD_SHADOW
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

	#define SIMUL_BUFFER_REGISTER(b)
	#define SIMUL_SAMPLER_REGISTER(s)
	#define SIMUL_TEXTURE_REGISTER(t)
	#define SIMUL_RWTEXTURE_REGISTER(u)
	#define SIMUL_STATE_REGISTER(s)

	#define uniform_buffer ALIGN_16 cbuffer

	#define SIMUL_CONSTANT_BUFFER(name,buff_num) struct name {
	#define SIMUL_CONSTANT_BUFFER_END };

	struct mat2
	{
		float m[8];
		operator const float *()
		{
			return m;
		}
		void operator=(const float *u)
		{
			for(int i=0;i<4;i++)
				m[i]=u[i];
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
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201) // anonymous unions warning
#endif
	struct mat4
	{
		union
		{
			float m[16];
			struct
			{
				float        _11, _12, _13, _14;
				float        _21, _22, _23, _24;
				float        _31, _32, _33, _34;
				float        _41, _42, _43, _44;
			};
			struct
			{
				float        _m00, _m01, _m02, _m03;
				float        _m10, _m11, _m12, _m13;
				float        _m20, _m21, _m22, _m23;
				float        _m30, _m31, _m32, _m33;
			};
			float M[4][4];
		};
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
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	struct vec2
	{
		float x,y;
		vec2(float X=0.0,float Y=0.0)
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
		vec4(const float *v)
		{
			this->x=v[0];
			this->y=v[1];
			this->z=v[2];
			this->w=v[3];
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
	typedef unsigned int uint;
	struct uint2
	{
		unsigned x,y;
		uint2(unsigned x=0,unsigned y=0)
		{
			this->x=x;
			this->y=y;
		}
		uint2(const int *v)
		{
			operator=(v);
		}
		uint2(const unsigned *v)
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
		}
		void operator=(const unsigned *v)
		{
			x=v[0];
			y=v[1];
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