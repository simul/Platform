//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CPPSL_HS
#define CPPSL_HS
// Definitions shared across C++, HLSL, and GLSL!

#ifdef __cplusplus
	// required for sqrt
	#include <math.h>
	// required for std::min and max
	#include <algorithm> 
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

	#define SIMUL_TEMPLATIZED_CONSTANT_BUFFER(struct_name, buff_num) SIMUL_CONSTANT_BUFFER(struct_name,buff_num)
	#define SIMUL_TEMPLATIZED_CONSTANT_BUFFER_END(struct_name, name, buff_num) SIMUL_CONSTANT_BUFFER_END

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
	template<typename T> struct tvector2
	{
		T x, y;
		tvector2(T X=0.0,T Y=0.0)
			:x(X),y(Y)
		{
		}
		tvector2(const T *v)
			:x(v[0]),y(v[1])
		{
		}
		tvector2(const tvector2 &v)
			:x(v.x),y(v.y)
		{
		}
		bool operator==(const tvector2 &v) const
		{
			return (x==v.x&&y==v.y);
		}
		bool operator!=(const tvector2 &v) const
		{
			return (x!=v.x||y!=v.y);
		}
		operator const T *() const
		{
			return &x;
		}
		const tvector2& operator=(const T *v)
		{
			x=v[0];
			y=v[1];
			return *this;
		}
		const tvector2& operator=(const tvector2 &v)
		{
			x=v.x;
			y=v.y;
			return *this;
		}
		void operator+=(tvector2 v)
		{
			x+=v.x;
			y+=v.y;
		}
		tvector2& operator-=(tvector2 v)
		{
			x-=v.x;
			y-=v.y;
			return *this;
		}
		tvector2& operator*=(tvector2 v)
		{
			x*=v.x;
			y*=v.y;
			return *this;
		}
		tvector2& operator/=(tvector2 v)
		{
			x/=v.x;
			y/=v.y;
			return *this;
		}
		tvector2 operator+(tvector2 v) const
		{
			tvector2 r;
			r.x=x+v.x;
			r.y=y+v.y;
			return r;
		}
		tvector2 operator-(tvector2 v) const
		{
			tvector2 r;
			r.x=x-v.x;
			r.y=y-v.y;
			return r;
		}
		tvector2 operator*(T m) const
		{
			tvector2 r;
			r.x=x*m;
			r.y=y*m;
			return r;
		}
		tvector2 operator/(T m) const
		{
			tvector2 r;
			r.x=x/m;
			r.y=y/m;
			return r;
		}
		
		tvector2 operator*(tvector2 v) const
		{
			tvector2 r;
			r.x=x*v.x;
			r.y=y*v.y;
			return r;
		}
		tvector2 operator/(tvector2 v) const
		{
			tvector2 r;
			r.x=x/v.x;
			r.y=y/v.y;
			return r;
		}
		void operator*=(T m)
		{
			x*=m;
			y*=m;
		}
		void operator/=(T m)
		{
			x/=m;
			y/=m;
		}
		friend tvector2 operator*(T m,tvector2 v)
		{
			tvector2 r;
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
		template<typename U> tvector3(const tvector3<U> &u)
		{
			operator=(u);
		}
		operator T *()
		{
			return &x;
		}
		operator const T *() const
		{
			return &x;
		}
		bool operator==(const tvector3 &v) const
		{
			return (x==v.x&&y==v.y&&z==v.z);
		}
		bool operator!=(const tvector3 &v) const
		{
			return (x!=v.x||y!=v.y||z!=v.z);
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
		template<typename U> const tvector3 &operator=(const tvector3<U> &u)
		{
			x = (T)u.x;
			y = (T)u.y;
			z = (T)u.z;
			return *this;
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
	
	template<typename T> struct tvector4
	{
		union
		{
			struct {
				T x, y, z, w;
			};
			struct {
				tvector3<T> xyz;
				float w_;
			};
		};
		tvector4(T x=0,T y=0,T z=0,T w=0)
		{
			this->x=x;
			this->y=y;
			this->z=z;
			this->w=w;
		}
		tvector4(const tvector3<T> &v,T w)
		{
			this->x=v.x;
			this->y=v.y;
			this->z=v.z;
			this->w=w;
		}
		tvector4(const T *v)
		{
			this->x=v[0];
			this->y=v[1];
			this->z=v[2];
			this->w=v[3];
		}
		operator const T *()
		{
			return &x;
		}
		void operator=(const T *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
			w=v[3];
		}
		tvector4 operator*(T m)
		{
			tvector4 r(x*m,y*m,z*m,w*m);
			return r;
		}
		tvector4 operator*(const tvector4& v)
		{
			tvector4 r(x*v.x,y*v.y,z*v.z,w*v.w);
			return r;
		}
		tvector4 operator+(tvector4 v) const
		{
			tvector4 r;
			r.x=x+v.x;
			r.y=y+v.y;
			r.z=z+v.z;
			r.w=w+v.w;
			return r;
		}
		tvector4 operator-(tvector4 v) const
		{
			tvector4 r;
			r.x=x-v.x;
			r.y=y-v.y;
			r.z=z-v.z;
			r.w=w-v.w;
			return r;
		}
		void operator+=(tvector4 v)
		{
			x+=v.x;
			y+=v.y;
			z+=v.z;
			w+=v.w;
		}
		void operator-=(tvector4 v)
		{
			x-=v.x;
			y-=v.y;
			z-=v.z;
			w-=v.w;
		}
		void operator*=(T m)
		{
			x*=m;
			y*=m;
			z*=m;
			w*=m;
		}
		void operator/=(T m)
		{
			x/=m;
			y/=m;
			z/=m;
			w/=m;
		}
		const tvector4 &operator*=(tvector4 v)
		{
			x=x*v.x;
			y=y*v.y;
			x=z*v.z;
			w=w*v.w;
			return *this;
		}
		tvector4 operator/(tvector4 v) const
		{
			tvector4 r;
			r.x=x/v.x;
			r.y=y/v.y;
			r.x=z/v.z;
			r.w=w/v.w;
			return r;
		}
		tvector4 operator-() const
		{
			tvector4 r;
			r.x=-x;
			r.y=-y;
			r.x=-x;
			r.w=-w;
			return r;
		}
	};
	namespace std
	{
		template <typename T> tvector2<T> max(tvector2<T> a, tvector2<T> b)
		{
			return tvector2<T>(std::max(a.x, b.x), std::max(a.y, b.y));
		};
		template <typename T> tvector2<T> min(tvector2<T> a, tvector2<T> b)
		{
			return tvector2<T>(std::min(a.x, b.x), std::min(a.y, b.y));
		};
		template <typename T> tvector3<T> max(tvector3<T> a,tvector3<T> b)
			{
				return tvector3<T>(std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z));
			};
		template <typename T> tvector3<T> min(tvector3<T> a,tvector3<T> b)
			{
				return tvector3<T>(std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z));
			};
	}
	template<typename T> T length(const tvector2<T>& u)
	{
		T size = u.x * u.x + u.y * u.y ;
		return static_cast<T>(sqrt(static_cast<double>(size)));
	}
	template<typename T> T length(const tvector3<T>& u)
	{
		T size = u.x * u.x + u.y * u.y + u.z * u.z;
		return static_cast<T>(sqrt(static_cast<double>(size)));
	}
	template<typename T> tvector3<T> normalize(const tvector3<T>& u)
	{
		T l=length(u);
		if(l>0)
			return u/l;
		return u;
	}
	template<typename T>
	tvector3<T> lerp(tvector3<T> a, tvector3<T> b, T x)
	{
		tvector3 c = b*x + a*(T(1.0) - x);
		return c;
	}
	typedef tvector2<float> vec2;
	typedef tvector3<float> vec3;
	template<typename T> T cross(const tvector2<T>& a, const tvector2<T>& b)
	{
		return  a.x * b.y - b.x * a.y;
	}
	template<typename T> T dot(const tvector2<T>& a, const tvector2<T>& b)
	{
		return  a.x * b.x+a.y * b.y;
	}
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
	template<typename T> struct tmatrix4
	{
		union
		{
			float m[16];
			struct
			{
				T        _11, _12, _13, _14;
				T        _21, _22, _23, _24;
				T        _31, _32, _33, _34;
				T        _41, _42, _43, _44;
			};
			struct
			{
				T        _m00, _m01, _m02, _m03;
				T        _m10, _m11, _m12, _m13;
				T        _m20, _m21, _m22, _m23;
				T        _m30, _m31, _m32, _m33;
			};
			struct
			{
				T a, b, c, d;
				T e, f, g, h;
				T i, j, k, l;
				T mm, n, o, p;
			};
			T M[4][4];
		};
		
		operator const T *()
		{
			return m;
		}
		void operator=(const T *v)
		{
			for(int i=0;i<16;i++)
				m[i]=v[i];
		}
		bool operator==(const tmatrix4 &v) const
		{
			for(int i=0;i<16;i++)
				if(m[i]!=v.m[i])
				return false;
			return true;
		}
		bool operator!=(const tmatrix4 &v) const
		{
			return !operator==(v);
		}
		inline static void mul(tmatrix4<T> &r,const tmatrix4<T> &a,const tmatrix4<T> &b)
		{
			for(int i=0;i<4;i++)
			{
				for(int j=0;j<4;j++)
				{
					const T *m1row=&a.m[i*4+0];
					T t=0.f;
					int k=0;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];	++k;
					t+=m1row[k]*b.m[k*4+j];
					r.M[i][j]=t;
				}
			}
		}
		tmatrix4<T> operator* (const tmatrix4<T>& input) const
		{
			tvector4<T> input_i={input.a, input.e, input.i, input.mm};
			tvector4<T> input_j={input.b, input.f, input.j, input.n};
			tvector4<T> input_k={input.c, input.g, input.k, input.o};
			tvector4<T> input_l={input.d, input.h, input.l, input.p};

			tvector4<T> output_i = *this * input_i;
			tvector4<T> output_j = *this * input_j;
			tvector4<T> output_k = *this * input_k;
			tvector4<T> output_l = *this * input_l;

			tmatrix4<T> output={output_i.x, output_i.y, output_i.z, output_i.w
						,output_j.x, output_j.y, output_j.z, output_j.w
						,output_k.x, output_k.y, output_k.z, output_k.w
						,output_l.x, output_l.y, output_l.z, output_l.w};
			output.transpose();
			return output;
		}

		void operator*=(T c)
		{
			for(int i=0;i<16;i++)
				m[i]*=c;
		}
		inline tmatrix4<T> t() const
		{
			tmatrix4<T> tr;
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					tr.m[i*4+j]=m[j*4+i];
			return tr;
		}
		void transpose()
		{
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					if(i<j)
					{
						T temp=m[i*4+j];
						m[i*4+j]=m[j*4+i];
						m[j*4+i]=temp;
					}
		}
		static inline tmatrix4<T> identity()
		{
			tmatrix4<T> m;
			for(int i=0;i<4;i++)
				for(int j=0;j<4;j++)
					m.M[i][j]=0.0f;
			m._11=m._22=m._33=m._44=1.0f;
			return m;
		}
		static inline tmatrix4<T> translation(vec3 tr)
		{
			tmatrix4<T> m=identity();
			m._14 = tr.x;
			m._24 = tr.y;
			m._34 =tr.z;
			return m;
		}
		//! Invert an unscaled transformation matrix. This must be a premultiplication transform,
		//! with the translation in the 4th column.
		static inline tmatrix4<T> unscaled_inverse_transform(const tmatrix4<T> &M) 
		{
#ifdef _DEBUG
			assert(M._m30==0&&M._m31==0&&M._m32==0&&M._m33==1.0f);
#endif
			const tvector3<T> XX={M._m00,M._m10,M._m20};
			const tvector3<T> YY={M._m01,M._m11,M._m21};
			const tvector3<T> ZZ={M._m02,M._m12,M._m22};
			tmatrix4 Inv=M.t();
			const tvector3<T> xe={M._m03,M._m13,M._m23};

			Inv._m30=0;
			Inv._m31=0;
			Inv._m32=0;
			Inv._m03=-dot(xe,XX);
			Inv._m13=-dot(xe,YY);
			Inv._m23=-dot(xe,ZZ);
			Inv._m33=T(1);
			return Inv;
		}
		static inline tmatrix4<T> inverse(const tmatrix4<T> &M) 
		{
			tmatrix4<T> inv;
		
			T det;
			int i;

			inv.m[0] = M.m[5]  * M.m[10] * M.m[15] - 
					 M.m[5]  * M.m[11] * M.m[14] - 
					 M.m[9]  * M.m[6]  * M.m[15] + 
					 M.m[9]  * M.m[7]  * M.m[14] +
					 M.m[13] * M.m[6]  * M.m[11] - 
					 M.m[13] * M.m[7]  * M.m[10];

			inv.m[4] = -M.m[4]  * M.m[10] * M.m[15] + 
					  M.m[4]  * M.m[11] * M.m[14] + 
					  M.m[8]  * M.m[6]  * M.m[15] - 
					  M.m[8]  * M.m[7]  * M.m[14] - 
					  M.m[12] * M.m[6]  * M.m[11] + 
					  M.m[12] * M.m[7]  * M.m[10];

			inv.m[8] = M.m[4]  * M.m[9] * M.m[15] - 
					 M.m[4]  * M.m[11] * M.m[13] - 
					 M.m[8]  * M.m[5] * M.m[15] + 
					 M.m[8]  * M.m[7] * M.m[13] + 
					 M.m[12] * M.m[5] * M.m[11] - 
					 M.m[12] * M.m[7] * M.m[9];

			inv.m[12] = -M.m[4]  * M.m[9] * M.m[14] + 
					   M.m[4]  * M.m[10] * M.m[13] +
					   M.m[8]  * M.m[5] * M.m[14] - 
					   M.m[8]  * M.m[6] * M.m[13] - 
					   M.m[12] * M.m[5] * M.m[10] + 
					   M.m[12] * M.m[6] * M.m[9];

			inv.m[1] = -M.m[1]  * M.m[10] * M.m[15] + 
					  M.m[1]  * M.m[11] * M.m[14] + 
					  M.m[9]  * M.m[2] * M.m[15] - 
					  M.m[9]  * M.m[3] * M.m[14] - 
					  M.m[13] * M.m[2] * M.m[11] + 
					  M.m[13] * M.m[3] * M.m[10];

			inv.m[5] = M.m[0]  * M.m[10] * M.m[15] - 
					 M.m[0]  * M.m[11] * M.m[14] - 
					 M.m[8]  * M.m[2] * M.m[15] + 
					 M.m[8]  * M.m[3] * M.m[14] + 
					 M.m[12] * M.m[2] * M.m[11] - 
					 M.m[12] * M.m[3] * M.m[10];

			inv.m[9] = -M.m[0]  * M.m[9] * M.m[15] + 
					  M.m[0]  * M.m[11] * M.m[13] + 
					  M.m[8]  * M.m[1] * M.m[15] - 
					  M.m[8]  * M.m[3] * M.m[13] - 
					  M.m[12] * M.m[1] * M.m[11] + 
					  M.m[12] * M.m[3] * M.m[9];

			inv.m[13] = M.m[0]  * M.m[9] * M.m[14] - 
					  M.m[0]  * M.m[10] * M.m[13] - 
					  M.m[8]  * M.m[1] * M.m[14] + 
					  M.m[8]  * M.m[2] * M.m[13] + 
					  M.m[12] * M.m[1] * M.m[10] - 
					  M.m[12] * M.m[2] * M.m[9];

			inv.m[2] = M.m[1]  * M.m[6] * M.m[15] - 
					 M.m[1]  * M.m[7] * M.m[14] - 
					 M.m[5]  * M.m[2] * M.m[15] + 
					 M.m[5]  * M.m[3] * M.m[14] + 
					 M.m[13] * M.m[2] * M.m[7] - 
					 M.m[13] * M.m[3] * M.m[6];

			inv.m[6] = -M.m[0]  * M.m[6] * M.m[15] + 
					  M.m[0]  * M.m[7] * M.m[14] + 
					  M.m[4]  * M.m[2] * M.m[15] - 
					  M.m[4]  * M.m[3] * M.m[14] - 
					  M.m[12] * M.m[2] * M.m[7] + 
					  M.m[12] * M.m[3] * M.m[6];

			inv.m[10] = M.m[0]  * M.m[5] * M.m[15] - 
					  M.m[0]  * M.m[7] * M.m[13] - 
					  M.m[4]  * M.m[1] * M.m[15] + 
					  M.m[4]  * M.m[3] * M.m[13] + 
					  M.m[12] * M.m[1] * M.m[7] - 
					  M.m[12] * M.m[3] * M.m[5];

			inv.m[14] = -M.m[0]  * M.m[5] * M.m[14] + 
					   M.m[0]  * M.m[6] * M.m[13] + 
					   M.m[4]  * M.m[1] * M.m[14] - 
					   M.m[4]  * M.m[2] * M.m[13] - 
					   M.m[12] * M.m[1] * M.m[6] + 
					   M.m[12] * M.m[2] * M.m[5];

			inv.m[3] = -M.m[1] * M.m[6] * M.m[11] + 
					  M.m[1] * M.m[7] * M.m[10] + 
					  M.m[5] * M.m[2] * M.m[11] - 
					  M.m[5] * M.m[3] * M.m[10] - 
					  M.m[9] * M.m[2] * M.m[7] + 
					  M.m[9] * M.m[3] * M.m[6];

			inv.m[7] = M.m[0] * M.m[6] * M.m[11] - 
					 M.m[0] * M.m[7] * M.m[10] - 
					 M.m[4] * M.m[2] * M.m[11] + 
					 M.m[4] * M.m[3] * M.m[10] + 
					 M.m[8] * M.m[2] * M.m[7] - 
					 M.m[8] * M.m[3] * M.m[6];

			inv.m[11] = -M.m[0] * M.m[5] * M.m[11] + 
					   M.m[0] * M.m[7] * M.m[9] + 
					   M.m[4] * M.m[1] * M.m[11] - 
					   M.m[4] * M.m[3] * M.m[9] - 
					   M.m[8] * M.m[1] * M.m[7] + 
					   M.m[8] * M.m[3] * M.m[5];

			inv.m[15] = M.m[0] * M.m[5] * M.m[10] - 
					  M.m[0] * M.m[6] * M.m[9] - 
					  M.m[4] * M.m[1] * M.m[10] + 
					  M.m[4] * M.m[2] * M.m[9] + 
					  M.m[8] * M.m[1] * M.m[6] - 
					  M.m[8] * M.m[2] * M.m[5];

			det = M.m[0] * inv.m[0] + M.m[1] * inv.m[4] + M.m[2] * inv.m[8] + M.m[3] * inv.m[12];

			if (det == 0)
			{
				return inv;
			}

			det = 1.f / det;

			for (i = 0; i < 16; i++)
				inv.m[i] = inv.m[i] * det;
			return inv;
		}
	};
	typedef tmatrix4<float> mat4;
	inline mat4 mul(const mat4& a, const mat4& b)
	{
		mat4 r;
		mat4::mul(r, a, b);
		return r;
	}
	inline vec3 operator*(const mat4 &m,const vec3 &v)
	{
		vec3 r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z;
		return r;
	}
	template<typename T> tvector3<T> operator*(T m, tvector3<T> v)
	{
		tvector3<T> r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		return r;
	}
	typedef tvector4<float> vec4;
	template<typename T>
	tvector4<T> operator*(T m,const tvector4<T> &v)
	{
		tvector4<T> r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		r.w=m*v.w;
		return r;
	}
	template<typename T>
	tvector4<T> operator*(const tvector4<T> &v,T m)
	{
		tvector4<T> r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		r.w=m*v.w;
		return r;
	}
	template<typename T>
	tvector4<T> operator*(const tvector4<T> &v,const mat4 &m)
	{
		tvector4<T> r;
		r.x=m._11*v.x+m._21*v.y+m._31*v.z+m._41*v.w;
		r.y=m._12*v.x+m._22*v.y+m._32*v.z+m._42*v.w;
		r.z=m._13*v.x+m._23*v.y+m._33*v.z+m._43*v.w;
		r.w=m._14*v.x+m._24*v.y+m._34*v.z+m._44*v.w;
		return r;
	}
	template<typename T>
	tvector4<T> operator*(const mat4 &m,const tvector4<T> &v)
	{
		tvector4<T> r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z+m._14*v.w;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z+m._24*v.w;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z+m._34*v.w;
		r.w=m._41*v.x+m._42*v.y+m._43*v.z+m._44*v.w;
		return r;
	}
	template<typename T>
	tvector4<T> lerp(tvector4<T> a, tvector4<T> b, T x)
	{
		tvector4 c = b*x + a*(T(1.0) - x);
		return c;
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
	inline vec3d cross(const vec3d &a,const vec3d &b)
	{
		vec3d r;
		r.x=a.y*b.z-b.y*a.z;
		r.y=a.z*b.x-b.z*a.x;
		r.z=a.x*b.y-b.x*a.y;
		return r;
	}
	template<typename T> T dot(const tvector3<T> &a,const tvector3<T> &b)
	{
		T c;
		c=a.x*b.x+a.y*b.y+a.z*b.z;
		return c;
	}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif

#endif