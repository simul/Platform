#ifndef VEC4_SL
#define VEC4_SL

template <typename T>
struct tvector4
{
	// Members
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

	// Constructors
	tvector4()
	{
		x = T(0);
		y = T(0);
		z = T(0);
		w = T(0);
	}
	tvector4(T x, T y, T z, T w)
	{
		this->x = x;
		this->y = y;
		this->z = z;
		this->w = w;
	}
	tvector4(const tvector3<T> &v, T w)
	{
		this->x = v.x;
		this->y = v.y;
		this->z = v.z;
		this->w = w;
	}
	tvector4(const T *v)
	{
		x = v[0];
		y = v[1];
		z = v[2];
		w = v[3];
	}
	tvector4(const tvector4 &v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
		w = v.w;
	}
	template <typename U>
	tvector4(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
		z = T(v[2]);
		w = T(v[3]);
	}
	template <typename U>
	tvector4(const tvector4<U> &v)
	{
		x = T(v.x);
		y = T(v.y);
		z = T(v.z);
		w = T(v.w);
	}

	// Equality operators
	bool operator==(const tvector4 &v) const
	{
		return (x == v.x && y == v.y && z == v.z && z == v.w);
	}
	bool operator!=(const tvector4 &v) const
	{
		return (x != v.x || y != v.y || z != v.z || w != v.w);
	}

	// Implicit conversions
	operator T *()
	{
		return &x;
	}
	operator const T *() const
	{
		return &x;
	}

	// Assignment operators
	tvector4 &operator=(const T *v)
	{
		x = v[0];
		y = v[1];
		z = v[2];
		w = v[3];
		return *this;
	}
	tvector4 &operator=(const tvector4 &v)
	{
		this->x = v.x;
		this->y = v.y;
		this->z = v.z;
		this->w = v.w;
		return *this;
	}
	template <typename U>
	const tvector4 &operator=(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
		z = T(v[2]);
		w = T(v[3]);
		return *this;
	}
	template <typename U>
	const tvector4 &operator=(const tvector4<U> &v)
	{
		this->x = T(v.x);
		this->y = T(v.y);
		this->z = T(v.z);
		this->w = T(v.w);
		return *this;
	}

	// Assignment operators by vector
	tvector4 &operator+=(const tvector4 &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
		w += v.w;
		return *this;
	}
	tvector4 &operator-=(const tvector4 &v)
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
		w -= v.w;
		return *this;
	}
	tvector4 &operator*=(const tvector4 &v)
	{
		x = x * v.x;
		y = y * v.y;
		z = z * v.z;
		w = w * v.w;
		return *this;
	}
	tvector4 &operator/=(const tvector4 &v)
	{
		x = x / v.x;
		y = y / v.y;
		z = z / v.z;
		w = w / v.w;
		return *this;
	}

	// Arithmetic operators by vector
	tvector4 operator+(const tvector4 &v) const
	{
		tvector4 r;
		r.x = x + v.x;
		r.y = y + v.y;
		r.z = z + v.z;
		r.w = w + v.w;
		return r;
	}
	tvector4 operator-(const tvector4 &v) const
	{
		tvector4 r;
		r.x = x - v.x;
		r.y = y - v.y;
		r.z = z - v.z;
		r.w = w - v.w;
		return r;
	}
	tvector4 operator*(const tvector4 &v) const
	{
		tvector4 r;
		r.x = x * v.x;
		r.y = y * v.y;
		r.z = z * v.z;
		r.w = w * v.w;
		return r;
	}
	tvector4 operator/(const tvector4 &v) const
	{
		tvector4 r;
		r.x = x / v.x;
		r.y = y / v.y;
		r.z = z / v.z;
		r.w = w / v.w;
		return r;
	}

	// Assignment operators by scalar
	tvector4 &operator*=(const T &m)
	{
		x *= m;
		y *= m;
		z *= m;
		w *= m;
		return *this;
	}
	tvector4 &operator/=(const T &m)
	{
		x /= m;
		y /= m;
		z /= m;
		w /= m;
		return *this;
	}

	// Arithmetic operators by scalar
	tvector4 operator*(const T &m) const
	{
		tvector4 r;
		r.x = x * m;
		r.y = y * m;
		r.z = z * m;
		r.w = w * m;
		return r;
	}
	tvector4 operator/(const T &m) const
	{
		tvector4 r;
		r.x = x / m;
		r.y = y / m;
		r.z = z / m;
		r.w = w / m;
		return r;
	}

	// Unary operators
	tvector4 operator+() const
	{
		tvector4 r;
		r.x = +x;
		r.y = +y;
		r.z = +z;
		r.w = +w;
		return r;
	}
	tvector4 operator-() const
	{
		tvector4 r;
		r.x = -x;
		r.y = -y;
		r.z = -z;
		r.w = -w;
		return r;
	}

	// Left hand arithmetics operators
	friend tvector4 operator*(const T &m, const tvector4 &v)
	{
		tvector r;
		r = v * m;
		return r;
	}
	friend tvector4 operator/(const T &m, const tvector4 &v)
	{
		tvector3 r;
		r.x = m / v.x;
		r.y = m / v.y;
		r.z = m / v.z;
		r.w = m / v.w;
		return r;
	}
};

typedef tvector4<float> vec4;
typedef tvector4<double> vec4d;
typedef tvector4<int> int4;
typedef tvector4<unsigned int> uint4;

#endif