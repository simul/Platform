#ifndef CPPSL_HS
#define CPPSL_HS

// Definitions shared across C++, HLSL, and GLSL!

#ifndef __cplusplus
	#define ALIGN_16
#endif

#ifdef __cplusplus
	#define ALIGN_16 __declspec(align(16))

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

#endif