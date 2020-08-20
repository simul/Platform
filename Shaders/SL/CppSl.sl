//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CPPSL_HS
#define CPPSL_HS
// Definitions shared across C++, HLSL, and GLSL!

#ifdef __cplusplus
#ifdef _MSC_VER
	#pragma warning(push)
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

	#define constant_buffer ALIGN_16 cbuffer
#ifdef _MSC_VER
	#define prag(c) __pragma(c)
#else
	#define prag(c) _Pragma(#c)
#endif
	#define SIMUL_CONSTANT_BUFFER(name,buff_num) prag(pack(push)) \
												prag(pack(4)) \
												struct name {static const int bindingIndex=buff_num;
	#define SIMUL_CONSTANT_BUFFER_END };\
									prag(pack(pop))

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
#pragma warning(disable:4201) // anonymous unions warning
#endif
	struct vec2
	{
		float x,y;
		vec2(float X=0.0,float Y=0.0)
			:x(X),y(Y)
		{
		}
		vec2(const float *v)
			:x(v[0]),y(v[1])
		{
		}
		vec2(const vec2 &v)
			:x(v.x),y(v.y)
		{
		}
		bool operator==(const vec2 &v) const
		{
			return (x==v.x&&y==v.y);
		}
		bool operator!=(const vec2 &v) const
		{
			return (x!=v.x||y!=v.y);
		}
		operator const float *() const
		{
			return &x;
		}
		const vec2& operator=(const float *v)
		{
			x=v[0];
			y=v[1];
			return *this;
		}
		const vec2& operator=(const vec2 &v)
		{
			x=v.x;
			y=v.y;
			return *this;
		}
		void operator+=(vec2 v)
		{
			x+=v.x;
			y+=v.y;
		}
		vec2& operator-=(vec2 v)
		{
			x-=v.x;
			y-=v.y;
			return *this;
		}
		vec2& operator*=(vec2 v)
		{
			x*=v.x;
			y*=v.y;
			return *this;
		}
		vec2& operator/=(vec2 v)
		{
			x/=v.x;
			y/=v.y;
			return *this;
		}
		vec2 operator+(vec2 v) const
		{
			vec2 r;
			r.x=x+v.x;
			r.y=y+v.y;
			return r;
		}
		vec2 operator-(vec2 v) const
		{
			vec2 r;
			r.x=x-v.x;
			r.y=y-v.y;
			return r;
		}
		vec2 operator*(float m) const
		{
			vec2 r;
			r.x=x*m;
			r.y=y*m;
			return r;
		}
		vec2 operator/(float m) const
		{
			vec2 r;
			r.x=x/m;
			r.y=y/m;
			return r;
		}
		
		vec2 operator*(vec2 v) const
		{
			vec2 r;
			r.x=x*v.x;
			r.y=y*v.y;
			return r;
		}
		vec2 operator/(vec2 v) const
		{
			vec2 r;
			r.x=x/v.x;
			r.y=y/v.y;
			return r;
		}
		void operator*=(float m)
		{
			x*=m;
			y*=m;
		}
		void operator/=(float m)
		{
			x/=m;
			y/=m;
		}
		friend vec2 operator*(float m,vec2 v)
		{
			vec2 r;
			r.x=v.x*m;
			r.y=v.y*m;
			return r;
		}
	};
	template<typename T> struct tvector3
	{
		T x,y,z;
		tvector3()
		{
		}
		tvector3(T x,T y,T z)
		{
			this->x=x;
			this->y=y;
			this->z=z;
		}
		tvector3(const T *v)
		{
			operator=(v);
		}
		tvector3(const int *v)
		{
			operator=(v);
		}
		operator T *()
		{
			return &x;
		}
		operator const T *() const
		{
			return &x;
		}
		void operator=(const int *v)
		{
			x=T(v[0]);
			y=T(v[1]);
			z=T(v[2]);
		}
		void operator=(const T *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
		}
		void operator*=(T m)
		{
			x*=m;
			y*=m;
			z*=m;
		}
		void operator/=(T m)
		{
			x/=m;
			y/=m;
			z/=m;
		}
		tvector3 operator-() const
		{
			tvector3 r;
			r.x=-x;
			r.y=-y;
			r.z=-z;
			return r;
		}
		tvector3 operator*(T m) const
		{
			tvector3 r;
			r.x=m*x;
			r.y=m*y;
			r.z=m*z;
			return r;
		}
		tvector3 operator/(T m) const
		{
			tvector3 r;
			r.x=x/m;
			r.y=y/m;
			r.z=z/m;
			return r;
		}
		tvector3 operator*(tvector3 v) const
		{
			tvector3 r;
			r.x=v.x*x;
			r.y=v.y*y;
			r.z=v.z*z;
			return r;
		}
		tvector3 operator/(tvector3 v) const
		{
			tvector3 r;
			r.x=x/v.x;
			r.y=y/v.y;
			r.z=z/v.z;
			return r;
		}
		tvector3 operator+(tvector3 v) const
		{
			tvector3 r;
			r.x=x+v.x;
			r.y=y+v.y;
			r.z=z+v.z;
			return r;
		}
		tvector3 operator-(tvector3 v) const
		{
			tvector3 r;
			r.x=x-v.x;
			r.y=y-v.y;
			r.z=z-v.z;
			return r;
		}
		void operator*=(const T *v)
		{
			x*=v[0];
			y*=v[1];
			z*=v[2];
		}
		void operator+=(const T *v)
		{
			x+=v[0];
			y+=v[1];
			z+=v[2];
		}
		void operator-=(const T *v)
		{
			x-=v[0];
			y-=v[1];
			z-=v[2];
		}
	};
	typedef tvector3<float> vec3;
	inline vec3 cross(const vec3 &a,const vec3 &b)
	{
		vec3 r;
		r.x=a.y*b.z-b.y*a.z;
		r.y=a.z*b.x-b.z*a.x;
		r.z=a.x*b.y-b.x*a.y;
		return r;
	}
	struct mat3
	{
		union
		{
			float m[16];
			struct
			{
				float        _11, _12, _13;
				float        _21, _22, _23;
				float        _31, _32, _33;
			};
			struct
			{
				float        _m00, _m01, _m02;
				float        _m10, _m11, _m12;
				float        _m20, _m21, _m22;
			};
			float M[3][3];
		};
	};
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
		inline static void mul(mat4 &r,const mat4 &a,const mat4 &b)
		{
			for(int i=0;i<4;i++)
			{
				for(int j=0;j<4;j++)
				{
					const float *m1row=&a.m[i*4+0];
					float t=0.f;
					int k=0;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];
					r.M[i][j]=t;
				}
			}
		}
		void operator*=(float c)
		{
			for(int i=0;i<16;i++)
				m[i]*=c;
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
		static inline mat4 identity()
		{
			mat4 m;
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					m.M[i][j]=0.0f;
			m._11=m._22=m._33=m._44=1.0f;
			return m;
		}
		static inline mat4 translation(vec3 tr)
		{
			mat4 m=identity();
			m._41=tr.x;
			m._42=tr.y;
			m._43=tr.z;
			return m;
		}
	};
	inline vec3 operator*(const mat4 &m,const vec3 &v)
	{
		vec3 r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z;
		return r;
	}
	inline vec3 operator*(float m,vec3 v)
	{
		vec3 r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		return r;
	}
	inline float dot(const vec3 &a,const vec3 &b)
	{
		float c;
		c=a.x*b.x+a.y*b.y+a.z*b.z;
		return c;
	}
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
		vec4(vec3 v,float w)
		{
			this->x=v.x;
			this->y=v.y;
			this->z=v.z;
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
		vec4 operator*(float m)
		{
			vec4 r(x*m,y*m,z*m,w*m);
			return r;
		}
		vec4 operator+(vec4 v) const
		{
			vec4 r;
			r.x=x+v.x;
			r.y=y+v.y;
			r.z=z+v.z;
			r.w=w+v.w;
			return r;
		}
		vec4 operator-(vec4 v) const
		{
			vec4 r;
			r.x=x-v.x;
			r.y=y-v.y;
			r.z=z-v.z;
			r.w=w-v.w;
			return r;
		}
		void operator+=(vec4 v)
		{
			x+=v.x;
			y+=v.y;
			z+=v.z;
			w+=v.w;
		}
		void operator-=(vec4 v)
		{
			x-=v.x;
			y-=v.y;
			z-=v.z;
			w-=v.w;
		}
		void operator*=(float m)
		{
			x*=m;
			y*=m;
			z*=m;
			w*=m;
		}
		void operator/=(float m)
		{
			x/=m;
			y/=m;
			z/=m;
			w/=m;
		}
		void operator*=(const float *v)
		{
			x*=v[0];
			y*=v[1];
			z*=v[2];
			w*=v[3];
		}
	};
	inline vec4 operator*(float m,const vec4 &v)
	{
		vec4 r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		r.w=m*v.w;
		return r;
	}
	inline vec4 operator*(const vec4 &v,float m)
	{
		vec4 r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		r.w=m*v.w;
		return r;
	}
	inline vec4 operator*(const vec4 &v,const mat4 &m)
	{
		vec4 r;
		r.x=m._11*v.x+m._21*v.y+m._31*v.z+m._41*v.w;
		r.y=m._12*v.x+m._22*v.y+m._32*v.z+m._42*v.w;
		r.z=m._13*v.x+m._23*v.y+m._33*v.z+m._43*v.w;
		r.w=m._14*v.x+m._24*v.y+m._34*v.z+m._44*v.w;
		return r;
	}
	inline vec4 operator*(const mat4 &m,const vec4 &v)
	{
		vec4 r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z+m._14*v.w;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z+m._24*v.w;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z+m._34*v.w;
		r.w=m._41*v.x+m._42*v.y+m._43*v.z+m._44*v.w;
		return r;
	}
	struct int2
	{
		int x,y;
		int2(int x=0,int y=0)
		{
			this->x=x;
			this->y=y;
		}
		int2(const int2& c)
		{
			this->x=c.x;
			this->y=c.y;
		}
		int2(const int *v)
		{
			operator=(v);
		}
		int2(const unsigned *v)
		{
			operator=(v);
		}
		operator const int *() const
		{
			return &x;
		}
		operator vec2() const
		{
			return vec2(float(x),float(y));
		}
		const int2& operator=(const int2 &v)
		{
			x=v.x;
			y=v.y;
			return *this;
		}
		const int2& operator=(const int *v)
		{
			x=v[0];
			y=v[1];
			return *this;
		}
		const int2& operator=(const unsigned *v)
		{
			x=v[0];
			y=v[1];
			return *this;
		}
		int2 operator+(const int2 &v) const
		{
			return int2(x+v.x,y+v.y);
		}
		int2 operator-(const int2 &v) const
		{
			return int2(x-v.x,y-v.y);
		}
		int2 operator*(int v)
		{
			return int2((x*v), (y*v));
		}
		int2 operator/(int v)
		{
			return int2((x/v), (y/v));
		}
		const int2 & operator/=(int v)
		{
			x /= v;
			y /=v;
			return *this;
		}
		int2 operator*(float v)
		{
			return int2(int(x*v),int(y*v));
		}
		bool operator==(const int2 &v) const
		{
			return (this->x==v.x)&&(this->y==v.y);
		}
		bool operator!=(const int2 &v) const
		{
			return (this->x!=v.x)||(this->y!=v.y);
		}
		friend int2 operator*(int m,int2 v)
		{
			int2 r;
			r.x=v.x*m;
			r.y=v.y*m;
			return r;
		}
		const int2& operator+=(int2 v)
		{
			x+=v.x;
			y+=v.y;
			return *this;
		}
		const int2& operator-=(int2 v)
		{
			x-=v.x;
			y-=v.y;
			return *this;
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
		uint2(const float *v)
		{
			x=uint(v[0]);
			y=uint(v[1]);
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
		bool operator==(const uint2 &v) const
		{
			return (x==v.x&&y==v.y);
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
		bool operator==(const uint3 &v) const
		{
			return (x==v.x&&y==v.y&&z==v.z);
		}
	};
	struct int3
	{
		int x,y,z;
		int3(int x=0,int y=0,int z=0)
		{
			this->x=x;
			this->y=y;
			this->z=z;
		}
		int3(uint3 v)
		{
			x=v.x;
			y=v.y;
			z=v.z;
		}
		int3(const int *v)
		{
			operator=(v);
		}
		int3(const float *v)
		{
			operator=(v);
		}
		operator const int *()
		{
			return &x;
		}
		int3 operator*(int m) const
		{
			int3 ret;
			ret.x=m*x;
			ret.y=m*y;
			ret.z=m*z;
			return ret;
		}
		int3 operator+(int3 v2) const
		{
			int3 ret;
			ret.x=x+v2.x;
			ret.y=y+v2.y;
			ret.z=z+v2.z;
			return ret;
		}
		void operator=(const int *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
		}
		void operator=(const float *v)
		{
			x=int(v[0]);
			y=int(v[1]);
			z=int(v[2]);
		}
		void operator*=(int u)
		{
			x*=u;
			y*=u;
			z*=u;
		}
		void operator/=(int u)
		{
			x/=u;
			y/=u;
			z/=u;
		}
		bool operator==(const int3 &v) const
		{
			return (x==v.x&&y==v.y&&z==v.z);
		}
	};
	
	struct int4
	{
		int x,y,z,w;
		int4(int x=0,int y=0,int z=0,int w=0)
		{
			this->x=x;
			this->y=y;
			this->z=z;
			this->w=w;
		}
		int4(const int *v)
		{
			operator=(v);
		}
		int4(const float *v)
		{
			operator=(v);
		}
		operator const int *()
		{
			return &x;
		}
		void operator=(const int *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
			w=v[3];
		}
		void operator=(const float *v)
		{
			x=int(v[0]);
			y=int(v[1]);
			z=int(v[2]);
			w=int(v[3]);
		}
		void operator*=(int u)
		{
			x*=u;
			y*=u;
			z*=u;
			w*=u;
		}
		void operator/=(int u)
		{
			x/=u;
			y/=u;
			z/=u;
			w/=u;
		}
	};
	
	struct uint4
	{
		uint x,y,z,w;
		uint4(uint x=0,uint y=0,uint z=0,uint w=0)
		{
			this->x=x;
			this->y=y;
			this->z=z;
			this->w=w;
		}
		uint4(const uint *v)
		{
			operator=(v);
		}
		uint4(const float *v)
		{
			operator=(v);
		}
		uint4(int4 v)
		{
			x=v.x;
			y=v.y;
			z=v.z;
			w=v.w;
		}
		operator const uint *()
		{
			return &x;
		}
		const uint4 &operator=( int4 v)
		{
			x=v.x;
			y=v.y;
			z=v.z;
			w=v.w;
			return *this;
		}
		const uint4 &operator=(const uint *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
			w=v[3];
			return *this;
		}
		const uint4 &operator=(const float *v)
		{
			x=uint(v[0]);
			y=uint(v[1]);
			z=uint(v[2]);
			w=uint(v[3]);
			return *this;
		}
		void operator*=(uint u)
		{
			x*=u;
			y*=u;
			z*=u;
			w*=u;
		}
		void operator/=(uint u)
		{
			x/=u;
			y/=u;
			z/=u;
			w/=u;
		}
	};
	//! Very simple 3 vector of doubles.
	typedef tvector3<double> vec3d;

	inline void vec3d_to_vec3(vec3&v3,const vec3d& v)
	{
		v3= vec3(float(v.x), float(v.y), float(v.z));
	}
	inline void vec3_to_vec3d(vec3d&v3,const vec3& v)
	{
		v3= vec3d(double(v.x), double(v.y), double(v.z));
	}

	inline vec3d cross(const vec3d &a,const vec3d &b)
	{
		vec3d r;
		r.x=a.y*b.z-b.y*a.z;
		r.y=a.z*b.x-b.z*a.x;
		r.z=a.x*b.y-b.x*a.y;
		return r;
	}
	inline double dot(const vec3d &a,const vec3d &b)
	{
		double c;
		c=a.x*b.x+a.y*b.y+a.z*b.z;
		return c;
	}
	//! Very simple 4x4 matrix of doubles.
	struct mat4d
	{
		union
		{
			double m[16];
			struct
			{
				double	_11, _12, _13, _14;
				double	_21, _22, _23, _24;
				double	_31, _32, _33, _34;
				double	_41, _42, _43, _44;
			};
			struct
			{
				double	_m00, _m01, _m02, _m03;
				double	_m10, _m11, _m12, _m13;
				double	_m20, _m21, _m22, _m23;
				double	_m30, _m31, _m32, _m33;
			};
			double M[4][4];
		};
		operator double *()
		{
			return m;
		}
		operator const double *()
		{
			return m;
		}
		void operator=(const double *v)
		{
			for(int i=0;i<16;i++)
				m[i]=v[i];
		}
		void operator=(const float *v)
		{
			for(int i=0;i<16;i++)
				m[i]=(double)v[i];
		}
		void transpose()
		{
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					if(i<j)
					{
						double temp=m[i*4+j];
						m[i*4+j]=m[j*4+i];
						m[j*4+i]=temp;
					}
		}
		mat4d operator*(const mat4d &b)
		{
			mat4d r;
			for(int i=0;i<4;i++)
			{
				for(int j=0;j<4;j++)
				{
					double t=0.0f;
					for(int k=0;k<4;k++)
					{
						t+=M[i][k]*b.M[j][k];
					}
					r.M[i][j]=t;
				}
			}
			return r;
		}
		static inline mat4d Identity()
		{
			mat4d m;
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					m.M[i][j]=0.0f;
			m._11=m._22=m._33=m._44=1.0f;
			return m;
		}
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

#endif