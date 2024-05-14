//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef CPPSL_HS
#define CPPSL_HS
// Definitions shared across C++, HLSL, and GLSL!

#ifdef __cplusplus
	// required for sqrt
	#include <math.h>
	// required for std::min and max
	#include <algorithm> 
	// required for stream operators
	#include <sstream>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4324)
	#define ALIGN_16 __declspec( align( 16 ) )
	#define PLATFORM_PACKED 
#else
	#define ALIGN_16
	#define PLATFORM_PACKED __attribute__ ((packed,aligned(1)))
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
#define SIMUL_CONSTANT_BUFFER(name, buff_num)     \
	prag(pack(push))                              \
		prag(pack(4)) struct name                 \
	{                                             \
		static const int bindingIndex = buff_num; \
												static const uint8_t groupIndex=0;

#define PLATFORM_GROUPED_CONSTANT_BUFFER(name, buff_num, group_num) \
	prag(pack(push))                                                \
		prag(pack(4)) struct name                                   \
	{                                                               \
		static const int bindingIndex = buff_num;                   \
												static const uint8_t groupIndex = group_num;

#define PLATFORM_NAMED_CONSTANT_BUFFER(struct_name, instance_name, buff_num, group_num) \
	prag(pack(push))                                                                    \
		prag(pack(4)) struct struct_name                                                \
	{                                                                                   \
		static const int bindingIndex = buff_num;                                       \
												static const uint8_t groupIndex = group_num;
#define SIMUL_CONSTANT_BUFFER_END \
	}                             \
	;                             \
									prag(pack(pop))
	#define PLATFORM_NAMED_CONSTANT_BUFFER_END SIMUL_CONSTANT_BUFFER_END
	#define PLATFORM_GROUPED_CONSTANT_BUFFER_END SIMUL_CONSTANT_BUFFER_END

#ifdef _MSC_VER
#pragma warning(disable:4201) // anonymous unions warning
#endif

	//////////////////
	//Vector Classes//
	//////////////////

	template <typename T>
	struct tvector2
	{
		T x, y;
		tvector2()
		{
			x = T(0);
			y = T(0);
		}
		tvector2(T X, T Y)
			:x(X),y(Y)
		{
		}
		tvector2(const T *v)
		{
			x = v[0];
			y = v[1];
		}
		tvector2(const tvector2<T> &v)
		{
			x = v.x;
			y = v.y;
		}
		template<typename U>
		tvector2(const U *v)
		{
			x=T(v[0]);
			y=T(v[1]);
		}
		template<typename U>
		tvector2(const tvector2<U> &v)
		{
			x = T(v.x);
			y = T(v.y);
		}
		bool operator==(const tvector2 &v) const
		{
			return (x==v.x&&y==v.y);
		}
		bool operator!=(const tvector2 &v) const
		{
			return (x!=v.x||y!=v.y);
		}
		operator T *()
		{
			return &x;
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
		template<typename U>
		const tvector2& operator=(const U *v)
		{
			x=T(v[0]);
			y=T(v[1]);
			return *this;
		}
		template<typename U>
		const tvector2& operator=(const tvector2<U> &v)
		{
			x=T(v.x);
			y=T(v.y);
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
	template <typename T>
	struct tvector3
	{
		T x, y, z;
		tvector3()
		{
			x = T(0);
			y = T(0);
			z = T(0);
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
		tvector3(const tvector3<T> &u)
		{
			operator=(u);
		}
		template <typename U>
		tvector3(const U *v)
		{
			operator=(v);
		}
		template<typename U> 
		tvector3(const tvector3<U> &u)
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
		const tvector3& operator=(const T *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
			return *this;
		}
		const tvector3& operator=(const tvector3 &v)
		{
			x=v.x;
			y=v.y;
			z=v.z;
			return *this;
		}
		template <typename U>
		const tvector3& operator=(const U *v)
		{
			x=T(v[0]);
			y=T(v[1]);
			z=T(v[2]);
			return *this;
		}
		template <typename U>
		const tvector3 &operator=(const tvector3<U> &u)
		{
			x = (T)u.x;
			y = (T)u.y;
			z = (T)u.z;
			return *this;
		}
		template<typename U> 
		const tvector3 &operator=(const tvector2<U> &u)
		{
			x = (T)u.x;
			y = (T)u.y;
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
		void operator/=(const T *v)
		{
			x/=v[0];
			y/=v[1];
			z/=v[2];
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
	template <typename T>
	struct tvector4
	{
		union
		{
		struct
		{
				T x, y, z, w;
			};
		struct
		{
				tvector3<T> xyz;
				T w_;
			};
		};
		tvector4()
			:xyz()
		{
			x = T(0);
			y = T(0);
			z = T(0);
			w = T(0);
		}
		tvector4(T x,T y,T z,T w)
			:xyz()
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
			this->x = v[0];
			this->y = v[1];
			this->z = v[2];
			this->w = v[3];
		}
		tvector4(const tvector4 &u)
			: xyz()
		{
			this->x = u.x;
			this->y = u.y;
			this->z = u.z;
			this->w = u.w;
		}
		template<typename U>
		tvector4(const tvector4<U> &u)
			: xyz()
		{
			this->x = T(u.x);
			this->y = T(u.y);
			this->z = T(u.z);
			this->w = T(u.w);
		}
		template <typename U>
		tvector4(const U *v)
		{
			this->x=T(v[0]);
			this->y=T(v[1]);
			this->z=T(v[2]);
			this->w=T(v[3]);
		}
		operator T *()
		{
			return &x;
		}
		operator const T *() const
		{
			return &x;
		}
		const tvector4 &operator=(const T *v)
		{
			x=v[0];
			y=v[1];
			z=v[2];
			w=v[3];
			return *this;
		}
		const tvector4 &operator=(const tvector4 &u)
		{
			this->x=u.x;
			this->y=u.y;
			this->z=u.z;
			this->w=u.w;
			return *this;
		}
		template<typename U>
		const tvector4 &operator=(const U *v)
		{
			x=T(v[0]);
			y=T(v[1]);
			z=T(v[2]);
			w=T(v[3]);
			return *this;
		}
		template<typename U>
		const tvector4 &operator=(const tvector4<U> &u)
		{
			this->x=T(u.x);
			this->y=T(u.y);
			this->z=T(u.z);
			this->w=T(u.w);
			return *this;
		}
		tvector4 operator*(T m)
		{
			tvector4 r(x*m,y*m,z*m,w*m);
			return r;
		}
		tvector4 operator/(T m)
		{
			tvector4 r(x / m, y /m, z /m, w /m);
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
			z=z*v.z;
			w=w*v.w;
			return *this;
		}
		tvector4 operator/(tvector4 v) const
		{
			tvector4 r;
			r.x=x/v.x;
			r.y=y/v.y;
			r.z=z/v.z;
			r.w=w/v.w;
			return r;
		}
		tvector4 operator-() const
		{
			tvector4 r;
			r.x=-x;
			r.y=-y;
			r.z=-z;
			r.w=-w;
			return r;
		}
		bool operator==(const tvector4 &v) const
		{
			return (x == v.x && y == v.y && z == v.z && z == v.w);
		}
		bool operator!=(const tvector4 &v) const
		{
			return (x != v.x || y != v.y || z != v.z || w != v.w);
		}
	};

	//Common typedefs

	typedef unsigned int uint;

	typedef tvector2<float> vec2;
	typedef tvector3<float> vec3;
	typedef tvector4<float> vec4;

	typedef tvector2<int> int2;
	typedef tvector3<int> int3;
	typedef tvector4<int> int4;

	typedef tvector2<uint> uint2;
	typedef tvector3<uint> uint3;
	typedef tvector4<uint> uint4;

	// Stream operators
	
	template <typename T>
	std::ostream &operator<<(std::ostream &os, const tvector2<T> &v)
	{
		os << "(" << v.x << ", " << v.y << ")";
		return os;
	}
	template <typename T>
	std::ostream &operator<<(std::ostream &os, const tvector3<T> &v)
	{
		os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
		return os;
	}
	template <typename T>
	std::ostream &operator<<(std::ostream &os, const tvector4<T> &v)
	{
		os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
		return os;
	}

	// Min/Max

	template <typename T>
	tvector2<T> max(const tvector2<T>& a, const tvector2<T>& b)
	{
		return tvector2<T>(std::max(a.x, b.x), std::max(a.y, b.y));
	};
	template <typename T>
	tvector2<T> min(const tvector2<T>& a, const tvector2<T>& b)
	{
		return tvector2<T>(std::min(a.x, b.x), std::min(a.y, b.y));
	};

	template <typename T>
	tvector3<T> max(const tvector3<T>& a, const tvector3<T>& b)
	{
		return tvector3<T>(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	};
	template <typename T> 
	tvector3<T> min(const tvector3<T>& a, const tvector3<T>& b)
	{
		return tvector3<T>(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	};

	template <typename T>
	tvector4<T> max(const tvector4<T>& a, const tvector4<T>& b)
	{
		return tvector4<T>(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));
	};
	template <typename T>
	tvector4<T> min(const tvector4<T>& a, const tvector4<T>& b)
	{
		return tvector4<T>(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
	};

	//Clamp

	template <typename T>
	T clamp(const T &t, const T &mn, const T &mx)
	{
		return std::max(std::min(t, mx), mn);
	}
	template <typename T>
	tvector2<T> clamp(const tvector2<T> &t, const tvector2<T> &mn, const tvector2<T> &mx)
	{
		return max(min(t, mx), mn);
	}
	template <typename T>
	tvector3<T> clamp(const tvector3<T> &t, const tvector3<T> &mn, const tvector3<T> &mx)
	{
		return max(min(t, mx), mn);
	}
	template <typename T>
	tvector4<T> clamp(const tvector4<T> &t, const tvector4<T> &mn, const tvector4<T> &mx)
	{
		return max(min(t, mx), mn);
	}

	//Saturate

	template <typename T>
	T saturate(const T &t)
	{
		return clamp(t, T(0), T(1));
	}
	template <typename T>
	tvector2<T> saturate(const tvector2<T> &t)
	{
		return clamp(t, tvector2<T>(0, 0), tvector2<T>(1, 1));
	}
	template <typename T>
	tvector3<T> saturate(const tvector3<T> &t)
	{
		return clamp(t, tvector3<T>(0, 0, 0), tvector3<T>(1, 1, 1));
	}
	template <typename T>
	tvector4<T> saturate(const tvector4<T> &t)
	{
		return clamp(t, tvector4<T>(0, 0, 0, 0), tvector4<T>(1, 1, 1, 1));
	}

	//Frac

	template <typename T>
	T frac(T x)
	{
		//https://twitter.com/JarkkoPFC/status/836953879896076293?lang=en-GB
		int X = (int)x;
		if (x < 0)
			X--;
		x -= (T)X;
		return x;
	}
	
	template <typename T>
	tvector2<T> frac(tvector2<T> v)
	{
		return tvector2<T>(frac(v.x), frac(v.y));
	}

	template <typename T>
	tvector3<T> frac(tvector3<T> v)
	{
		return tvector3<T>(frac(v.x), frac(v.y), frac(v.z));
	}

	template <typename T>
	tvector4<T> frac(tvector4<T> v)
	{
		return tvector4<T>(frac(v.x), frac(v.y), frac(v.z), frac(v.w));
	}

	//Length

	template<typename T> 
	T length(const tvector2<T>& u)
	{
		T size = u.x * u.x + u.y * u.y ;
		return static_cast<T>(sqrt(static_cast<double>(size)));
	}
	template<typename T> 
	T length(const tvector3<T>& u)
	{
		T size = u.x * u.x + u.y * u.y + u.z * u.z;
		return static_cast<T>(sqrt(static_cast<double>(size)));
	}
	template <typename T>
	T length(const tvector4<T> &u)
	{
		T size = u.x * u.x + u.y * u.y + u.z * u.z + u.w * u.w;
		return static_cast<T>(sqrt(static_cast<double>(size)));
	}

	//Normalise

	template <typename T>
	tvector2<T> normalize(const tvector2<T> &u)
	{
		T l = length(u);
		if (l > 0)
			return u / l;
		return u;
	}
	template<typename T>
	tvector3<T> normalize(const tvector3<T>& u)
	{
		T l=length(u);
		if(l>0)
			return u/l;
		return u;
	}
	template <typename T>
	tvector4<T> normalize(const tvector4<T> &u)
	{
		T l = length(u);
		if (l > 0)
			return u / l;
		return u;
	}

	//Lerp

	template <typename T>
	T lerp(const T& a, const T& b, const T& x)
	{
		return b * x + a * (T(1.0) - x);
	}
	template <typename T>
	tvector2<T> lerp(const tvector2<T>& a, const tvector2<T>& b, const T& x)
	{
		tvector2 c = b * x + a * (T(1.0) - x);
		return c;
	}
	template<typename T>
	tvector3<T> lerp(const tvector3<T>& a, const tvector3<T>& b, const T& x)
	{
		tvector3 c = b * x + a * (T(1.0) - x);
		return c;
	}
	template <typename T>
	tvector4<T> lerp(const tvector4<T>& a, const tvector4<T>& b, const T& x)
	{
		tvector4 c = b * x + a * (T(1.0) - x);
		return c;
	}
	
	//Mat2 Determinant?
	template<typename T>
	T cross(const tvector2<T>& a, const tvector2<T>& b)
	{
		return  a.x * b.y - b.x * a.y;
	}

	//Cross

	template<typename T>
	inline tvector3<T> cross(const tvector3<T> &a, const tvector3<T> &b)
	{
		tvector3<T> r;
		r.x = a.y * b.z - b.y * a.z;
		r.y = a.z * b.x - b.z * a.x;
		r.z = a.x * b.y - b.x * a.y;
		return r;
	}

	//Dot

	template<typename T> 
	T dot(const tvector2<T>& a, const tvector2<T>& b)
	{
		return  a.x * b.x+a.y * b.y;
	}
	template <typename T>
	T dot(const tvector3<T> &a, const tvector3<T> &b)
	{
		return a.x *b.x + a.y *b.y + a.z *b.z;
	}
	template <typename T>
	T dot(const tvector4<T> &a, const tvector4<T> &b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}

	//abs

	template <typename T>
	tvector2<T> abs(const tvector2<T> &v)
	{
		return {abs(v.x), abs(v.y)};
	}
	template<typename T>
	tvector3<T> abs(const tvector3<T> &v)
	{
		return {abs(v.x), abs(v.y), abs(v.z)};
	}
	template <typename T>
	tvector4<T> abs(const tvector4<T> &v)
	{
		return {abs(v.x), abs(v.y), abs(v.z), abs(v.w)};
	}


	//////////////////
	//Matrices class//
	//////////////////

	struct mat2
	{
		float m[8];
		operator const float *() const
		{
			return m;
		}
		void operator=(const float *u)
		{
			for (int i = 0; i < 4; i++)
				m[i] = u[i];
		}
		void transpose()
		{
			for (int i = 0; i < 2; i++)
			{
				for (int j = 0; j < 2; j++)
				{
					if (i < j)
					{
						float temp = m[i * 2 + j];
						m[i * 2 + j] = m[j * 2 + i];
						m[j * 2 + i] = temp;
					}
				}
			}
		}
	};

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
	template <typename T>
	struct tmatrix4
	{
		union
		{
			T m[16];
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
		
		operator const T *() const
		{
			return m;
		}
		tmatrix4 operator+(const tmatrix4 &b)
		{
			tmatrix4 r;
			for(int i=0;i<16;i++)
				r.m[i]=m[i]+b.m[i];
			return r;
		}
		tmatrix4 operator-(const tmatrix4 &b)
		{
			tmatrix4 r;
			for(int i=0;i<16;i++)
				r.m[i]=m[i]-b.m[i];
			return r;
		}
		const tmatrix4& operator=(const T *v)
		{
			for(int i=0;i<16;i++)
				m[i]=v[i];
			return *this;
		}
		template<typename U> 
		const tmatrix4& operator=(const tmatrix4<U> &u)
		{
			for(int i=0;i<16;i++)
				m[i]=T(u.m[i]);
			return *this;
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
		static inline tmatrix4<T> translationColumnMajor(tvector3<T> tr)
		{
			tmatrix4<T> m=identity();
			m._14 = tr.x;
			m._24 = tr.y;
			m._34 =tr.z;
			return m;
		}
		tvector3<T> getTranslationRowMajor() const
		{
			tvector3<T> t;
			t={_m30,_m31,_m32};
			return t;
		}
		void setTranslationRowMajor(const tvector3<T> &t)
		{
			_m30=t.x;
			_m31=t.y;
			_m32=t.z;
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

			det = T(1)/ det;

			for (i = 0; i < 16; i++)
				inv.m[i] = inv.m[i] * det;
			return inv;
		}
		//! Confirm that the right-hand column xyz is zero.
		bool is_row_major() const
		{
			return(_m03==0&&_m13==0&&_m23==0);
		}
		//! Confirm that the bottom row xyz is zero.
		bool is_column_major() const
		{
			return(_m30==0&&_m31==0&&_m32==0);
		}
		/// Set the translation as the 4th column, assigning 1.0 to the 4,4 corner.
		template <typename U>
		void setColumnTranslation(const tvector3<U> &translation)
		{
			_m03 = translation.x;
			_m13 = translation.y;
			_m23 = translation.z;
			_m33 = T(1);
		}
		/// Set the rotation to the given quaternion, on the basis of a premultiplying transform matrix.
		template <typename U>
		void setRotation(const tvector4<U> &q)
		{
			_m00=(powf(q.w, 2) + powf(q.x, 2) - powf(q.y, 2) - powf(q.z, 2));
			_m01=2 * (q.x * q.y - q.z * q.w);
			_m02=2 * (q.x * q.z + q.y * q.w);
			
			_m10=2 * (q.x * q.y + q.z * q.w);
			_m11=(powf(q.w, 2) - powf(q.x, 2) + powf(q.y, 2) - powf(q.z, 2));
			_m12=2 * (q.y * q.z - q.x * q.w);

			_m20=2 * (q.x * q.z - q.y * q.w);
			_m21=2 * (q.y * q.z + q.x * q.w);
			_m22 = (powf(q.w, 2) - powf(q.x, 2) - powf(q.y, 2) + powf(q.z, 2));

			_m30 =0;
			_m31 =0;
			_m32 =0;
		}
		/// Set the rotation to the given quaternion, and the translation to the given vector, on the basis of a premultiplying transform matrix.
		template <typename U,typename V>
		void setRotationTranslation(const tvector4<U> &q, const tvector3<V> &translation)
		{
			T ww = q.w * q.w;
			T xx = q.x * q.x;
			T yy = q.y * q.y;
			T zz = q.z * q.z;
			T xy = q.x * q.y;
			T yz = q.y * q.z;
			T zx = q.z * q.x;
			T xw = q.x * q.w;
			T yw = q.y * q.w;
			T zw = q.z * q.w;
			_m00 = (ww + xx - yy - zz);
			_m01 = 2 * (xy - zw);
			_m02 = 2 * (zx + yw);
			_m03 = translation.x;

			_m10 = 2 * (xy + zw);
			_m11 = (ww - xx + yy - zz);
			_m12 = 2 * (yz - xw);
			_m13 = translation.y;

			_m20 = 2 * (zx - yw);
			_m21 = 2 * (yz + xw);
			_m22 = (ww - xx - yy + zz);
			_m23 = translation.z;

			_m30 = 0;
			_m31 = 0;
			_m32 = 0;
			_m33 = T(1);
		}
		/// Set the rotation to the given quaternion, the translation to the given vector, and the scale to the given scale,
		/// on the basis of a premultiplying transform matrix.
		template <typename U, typename V, typename W>
		void setRotationTranslationScale(const tvector4<U> &q, const tvector3<V> &translation, const tvector3<W> &scale)
		{
			T ww = q.w * q.w;
			T xx = q.x * q.x;
			T yy = q.y * q.y;
			T zz = q.z * q.z;
			T xy = q.x * q.y;
			T yz = q.y * q.z;
			T zx = q.z * q.x;
			T xw = q.x * q.w;
			T yw = q.y * q.w;
			T zw = q.z * q.w;
			_m00 = (ww + xx - yy - zz);
			_m01 = T(2) * (xy - zw);
			_m02 = T(2) * (zx + yw);
			_m03 = translation.x;

			_m10 = T(2) * (xy + zw);
			_m11 = (ww - xx + yy - zz);
			_m12 = T(2) * (yz - xw);
			_m13 = translation.y;

			_m20 = T(2) * (zx - yw);
			_m21 = T(2) * (yz + xw);
			_m22 = (ww - xx - yy + zz);
			_m23 = translation.z;

			_m30 = 0;
			_m31 = 0;
			_m32 = 0;
			_m33 = T(1);
			applyScale(scale);
		}
		
		template<typename U>
		static tmatrix4 translation(const tvector3<U>& translation)
		{
			return 
			{
				1.0f, 0.0f, 0.0f, translation.x,
				0.0f, 1.0f, 0.0f, translation.y,
				0.0f, 0.0f, 1.0f, translation.z,
				0.0f, 0.0f, 0.0f, 1.0f
			};
		}
		template <typename U>
		void applyScale(const tvector3<U> &scale)
		{
			_m00 *= scale.x;
			_m01 *= scale.y;
			_m02 *= scale.z;
			_m10 *= scale.x;
			_m11 *= scale.y;
			_m12 *= scale.z;
			_m20 *= scale.x;
			_m21 *= scale.y;
			_m22 *= scale.z;
		}
		template<typename U>
		static tmatrix4 rotation(const tvector4<U>& orientation)
		{
			return {
				(powf(orientation.w, 2) + powf(orientation.x, 2) - powf(orientation.y, 2) - powf(orientation.z, 2)), 2 * (orientation.x * orientation.y - orientation.z * orientation.w), 2 * (orientation.x * orientation.z + orientation.y * orientation.w), 0,
				2 * (orientation.x * orientation.y + orientation.z * orientation.w), (powf(orientation.w, 2) - powf(orientation.x, 2) + powf(orientation.y, 2) - powf(orientation.z, 2)), 2 * (orientation.y * orientation.z - orientation.x * orientation.w), 0,
				2 * (orientation.x * orientation.z - orientation.y * orientation.w), 2 * (orientation.y * orientation.z + orientation.x * orientation.w), (powf(orientation.w, 2) - powf(orientation.x, 2) - powf(orientation.y, 2) + powf(orientation.z, 2)), 0,
				0, 0, 0, T(1)
			};
		}
		template <typename U>
		static tmatrix4 scale(const tvector3<U> &scale)
		{
			return {
				scale.x, 0, 0, 0,
				0, scale.y, 0, 0,
				0, 0, scale.z, 0,
				0, 0, 0, T(1)};
		}
		//! Get the translation, if this matrix is a transform matrix for premultiplication
		//! (i.e. the translation is the rightmost column)
		tvector3<T> GetTranslation() const
		{
			return {_m03, _m13, _m23};
		}
		//! Get the scale, if this matrix is a transform matrix
		tvector3<T> GetScale() const
		{
			tvector3<T> x={_m00, _m10, _m20};
			tvector3<T> y={_m01, _m11, _m21};
			tvector3<T> z={_m02, _m12, _m22};

			return {length(x), length(y), length(z)};
		}
	};
	
	//Matrix/Vector multiply operators

	template<typename T> 
	tmatrix4<T> mul(const tmatrix4<T>& a, const tmatrix4<T>& b)
	{
		tmatrix4<T> r;
		tmatrix4<T>::mul(r, a, b);
		return r;
	}
	template <typename T>
	tvector3<T> operator*(const tmatrix4<T> &m, const tvector3<T> &v)
	{
		tvector3<T> r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z;
		return r;
	}
	template<typename T> 
	tvector3<T> operator*(T m, tvector3<T> v)
	{
		tvector3<T> r;
		r.x=m*v.x;
		r.y=m*v.y;
		r.z=m*v.z;
		return r;
	}
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
	tvector4<T> operator*(const tvector4<T> &v, const tmatrix4<T> &m)
	{
		tvector4<T> r;
		r.x=m._11*v.x+m._21*v.y+m._31*v.z+m._41*v.w;
		r.y=m._12*v.x+m._22*v.y+m._32*v.z+m._42*v.w;
		r.z=m._13*v.x+m._23*v.y+m._33*v.z+m._43*v.w;
		r.w=m._14*v.x+m._24*v.y+m._34*v.z+m._44*v.w;
		return r;
	}
	template<typename T>
	tvector4<T> operator*(const tmatrix4<T> &m, const tvector4<T> &v)
	{
		tvector4<T> r;
		r.x=m._11*v.x+m._12*v.y+m._13*v.z+m._14*v.w;
		r.y=m._21*v.x+m._22*v.y+m._23*v.z+m._24*v.w;
		r.z=m._31*v.x+m._32*v.y+m._33*v.z+m._34*v.w;
		r.w=m._41*v.x+m._42*v.y+m._43*v.z+m._44*v.w;
		return r;
	}

	// Others

	template <typename T>
	void closest_approach(const tvector3<T> A, const tvector3<T> B, tvector3<T> C, const tvector3<T> D, bool limit, tvector3<T> &P1, tvector3<T> &P2)
	{
		tvector3 u = B - A;
		tvector3 v = D - C;
		tvector3 w = A - C;

		T a = dot(u, u); // always >= 0
		T b = dot(u, v);
		T c = dot(v, v); // always >= 0
		T d = dot(u, w);
		T e = dot(v, w);
		T sc, sN, sD = a * c - b * b; // sc = sN / sD, sD >= 0
		T tc, tN, tD = a * c - b * b; // tc = tN / tD, tD >= 0
		T tol = T(1e-15);
		// compute the line parameters of the two closest points
		if (sD < tol)
		{			  // the lines are almost parallel
			sN = 0.0; // force using point A on segment AB
			sD = 1.0; // to prevent possible division by 0.0 later
			tN = e;
			tD = c;
		}
		else
		{ // get the closest points on the infinite lines
			sN = (b * e - c * d);
			tN = (a * e - b * d);
			if (limit)
			{
				if (sN < 0.0)
				{			  // sc < 0 => the s=0 edge is visible
					sN = 0.0; // compute shortest connection of A to segment CD
					tN = e;
					tD = c;
				}
				else if (sN > sD)
				{			 // sc > 1  => the s=1 edge is visible
					sN = sD; // compute shortest connection of B to segment CD
					tN = e + b;
					tD = c;
				}
			}
		}
		if (limit)
		{
			if (tN < 0.0)
			{ // tc < 0 => the t=0 edge is visible
				tN = 0.0;
				// recompute sc for this edge
				if (-d < 0.0) // compute shortest connection of C to segment AB
					sN = 0.0;
				else if (-d > a)
					sN = sD;
				else
				{
					sN = -d;
					sD = a;
				}
			}
			else if (tN > tD)
			{ // tc > 1  => the t=1 edge is visible
				tN = tD;
				// recompute sc for this edge
				if ((-d + b) < 0.0) // compute shortest connection of D to segment AB
					sN = 0;
				else if ((-d + b) > a)
					sN = sD;
				else
				{
					sN = (-d + b);
					sD = a;
				}
			}
		}
		// finally do the division to get sc and tc
		sc = (fabs(sN) < tol ? 0.0 : sN / sD);
		tc = (fabs(tN) < tol ? 0.0 : tN / tD);

		P1 = A + (sc * u);
		P2 = C + (tc * v);
	}

	typedef tvector4<float> vec4;
	typedef tvector2<float> vec2;
	typedef tvector3<float> vec3;

#ifndef EXCLUDE_PLATFORM_TYPEDEFS
	typedef tmatrix4<float> mat4;
	typedef tmatrix4<double> mat4d;
	
	typedef unsigned int uint;
	typedef tvector2<int32_t> int2;
	typedef tvector3<int32_t> int3;
	typedef tvector4<int32_t> int4;
	
	typedef tvector2<uint32_t> uint2;
	typedef tvector3<uint32_t> uint3;
	typedef tvector4<uint32_t> uint4;
	
	//! Very simple 2 vector of doubles.
	typedef tvector2<double> vec2d;
	//! Very simple 3 vector of doubles.
	typedef tvector3<double> vec3d;
	//! Very simple 4 vector of doubles.
	typedef tvector4<double> vec4d;
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif
#endif