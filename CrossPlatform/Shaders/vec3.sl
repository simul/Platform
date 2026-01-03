#ifndef VEC3_SL
#define VEC3_SL

template <typename T>
struct tvector3
{
	// Members
	T x, y, z;

	// Constructors
	tvector3()
	{
		x = T(0);
		y = T(0);
		z = T(0);
	}
	tvector3(T x, T y, T z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}
	tvector3(T v)
	{
		x = v;
		y = v;
		z = v;
	}
	tvector3(const T *v)
	{
		x = v[0];
		y = v[1];
		z = v[2];
	}
	tvector3(const tvector3<T> &v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}
	template <typename U>
	tvector3(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
		z = T(v[2]);
	}
	template <typename U>
	tvector3(const tvector3<U> &v)
	{
		x = T(v.x);
		y = T(v.y);
		z = T(v.z);
	}

	// Equality operators
	bool operator==(const tvector3 &v) const
	{
		return (x == v.x && y == v.y && z == v.z);
	}
	bool operator!=(const tvector3 &v) const
	{
		return (x != v.x || y != v.y || z != v.z);
	}

	// Comparison operators
	bool operator<(const tvector3 &v) const
	{
		return (x < v.x && y < v.y && z < v.z);
	}
	bool operator>(const tvector3 &v) const
	{
		return (x > v.x && y > v.y && z > v.z);
	}
	bool operator<=(const tvector3 &v) const
	{
		return (x <= v.x && y <= v.y && z <= v.z);
	}
	bool operator>=(const tvector3 &v) const
	{
		return (x >= v.x && y >= v.y && z >= v.z);
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
	const tvector3 &operator=(const T *v)
	{
		x = v[0];
		y = v[1];
		z = v[2];
		return *this;
	}
	const tvector3 &operator=(const tvector3 &v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}
	template <typename U>
	const tvector3 &operator=(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
		z = T(v[2]);
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
	template <typename U>
	const tvector3 &operator=(const tvector2<U> &u)
	{
		x = (T)u.x;
		y = (T)u.y;
		return *this;
	}

	// Assignment operators by vector
	tvector3 &operator+=(const tvector3 &v)
	{
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	tvector3 &operator-=(const tvector3 &v)
	{
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}
	tvector3 &operator*=(const tvector3 &v)
	{
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}
	tvector3 &operator/=(const tvector3 &v)
	{
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}

	// Arithmetic operators by vector
	tvector3 operator+(const tvector3 &v) const
	{
		tvector3 r;
		r.x = x + v.x;
		r.y = y + v.y;
		r.z = z + v.z;
		return r;
	}
	tvector3 operator-(const tvector3 &v) const
	{
		tvector3 r;
		r.x = x - v.x;
		r.y = y - v.y;
		r.z = z - v.z;
		return r;
	}
	tvector3 operator*(const tvector3 &v) const
	{
		tvector3 r;
		r.x = v.x * x;
		r.y = v.y * y;
		r.z = v.z * z;
		return r;
	}
	tvector3 operator/(const tvector3 &v) const
	{
		tvector3 r;
		r.x = x / v.x;
		r.y = y / v.y;
		r.z = z / v.z;
		return r;
	}

	// Assignment operators by scalar
	tvector3 &operator*=(const T &m)
	{
		x *= m;
		y *= m;
		z *= m;
		return *this;
	}
	tvector3 &operator/=(const T &m)
	{
		x /= m;
		y /= m;
		z /= m;
		return *this;
	}

	// Arithmetic operators by scalar
	tvector3 operator*(const T &m) const
	{
		tvector3 r;
		r.x = m * x;
		r.y = m * y;
		r.z = m * z;
		return r;
	}
	tvector3 operator/(const T &m) const
	{
		tvector3 r;
		r.x = x / m;
		r.y = y / m;
		r.z = z / m;
		return r;
	}

	// Unary operators
	tvector3 operator+() const
	{
		tvector3 r;
		r.x = +x;
		r.y = +y;
		r.z = +z;
		return r;
	}
	tvector3 operator-() const
	{
		tvector3 r;
		r.x = -x;
		r.y = -y;
		r.z = -z;
		return r;
	}

	//Left hand arithmetics operators
	friend tvector3 operator*(const T &m, const tvector3 &v)
	{
		tvector3 r;
		r = v * m;
		return r;
	}
	friend tvector3 operator/(const T &m, const tvector3 &v)
	{
		tvector3 r;
		r.x = m / v.x;
		r.y = m / v.y;
		r.z = m / v.z;
		return r;
	}
};

typedef tvector3<float> vec3;
typedef tvector3<double> vec3d;
typedef tvector3<int> int3;
typedef tvector3<unsigned int> uint3;

// POD-compatible packed version for use in packed structs
template <typename T>
struct pvector3
{
	T x, y, z;
};

typedef pvector3<float> vec3_packed;
typedef pvector3<double> vec3d_packed;
typedef pvector3<int> int3_packed;
typedef pvector3<unsigned int> uint3_packed;

template <typename T>
pvector3<T> packed(const tvector3<T> &v)
{
	return pvector3<T>{v.x, v.y, v.z};
}

#endif