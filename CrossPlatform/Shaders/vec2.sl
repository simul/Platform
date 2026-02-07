#ifndef VEC2_SL
#define VEC2_SL

template <typename T>
struct tvector2
{
	// Members
	T x, y;

	// Constructors
	tvector2()
	{
		x = T(0);
		y = T(0);
	}
	tvector2(T x, T y)
	{
		this->x = x;
		this->y = y;
	}
	tvector2(T v)
	{
		x = v;
		y = v;
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
	template <typename U>
	tvector2(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
	}
	template <typename U>
	tvector2(const tvector2<U> &v)
	{
		x = T(v.x);
		y = T(v.y);
	}

	// Equality operators
	bool operator==(const tvector2 &v) const
	{
		return (x == v.x && y == v.y);
	}
	bool operator!=(const tvector2 &v) const
	{
		return (x != v.x || y != v.y);
	}

	// Comparison operators
	bool operator<(const tvector2 &v) const
	{
		return (x < v.x && y < v.y);
	}
	bool operator>(const tvector2 &v) const
	{
		return (x > v.x && y > v.y);
	}
	bool operator<=(const tvector2 &v) const
	{
		return (x <= v.x && y <= v.y);
	}
	bool operator>=(const tvector2 &v) const
	{
		return (x >= v.x && y >= v.y);
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
	tvector2 &operator=(const T *v)
	{
		x = v[0];
		y = v[1];
		return *this;
	}
	tvector2 &operator=(const tvector2 &v)
	{
		x = v.x;
		y = v.y;
		return *this;
	}
	template <typename U>
	tvector2 &operator=(const U *v)
	{
		x = T(v[0]);
		y = T(v[1]);
		return *this;
	}
	template <typename U>
	tvector2 &operator=(const tvector2<U> &v)
	{
		x = T(v.x);
		y = T(v.y);
		return *this;
	}

	// Assignment operators by vector
	tvector2 &operator+=(const tvector2 &v)
	{
		x += v.x;
		y += v.y;
		return *this;
	}
	tvector2 &operator-=(const tvector2 &v)
	{
		x -= v.x;
		y -= v.y;
		return *this;
	}
	tvector2 &operator*=(const tvector2 &v)
	{
		x *= v.x;
		y *= v.y;
		return *this;
	}
	tvector2 &operator/=(const tvector2 &v)
	{
		x /= v.x;
		y /= v.y;
		return *this;
	}

	// Arithmetic operators by vector
	tvector2 operator+(const tvector2 &v) const
	{
		tvector2 r;
		r.x = x + v.x;
		r.y = y + v.y;
		return r;
	}
	tvector2 operator-(const tvector2 &v) const
	{
		tvector2 r;
		r.x = x - v.x;
		r.y = y - v.y;
		return r;
	}
	tvector2 operator*(const tvector2 &v) const
	{
		tvector2 r;
		r.x = x * v.x;
		r.y = y * v.y;
		return r;
	}
	tvector2 operator/(const tvector2 &v) const
	{
		tvector2 r;
		r.x = x / v.x;
		r.y = y / v.y;
		return r;
	}

	// Assignment operators by scalar
	tvector2 &operator*=(const T &m)
	{
		x *= m;
		y *= m;
		return *this;
	}
	tvector2 &operator/=(const T &m)
	{
		x /= m;
		y /= m;
		return *this;
	}

	// Arithmetic operators by scalar
	tvector2 operator*(const T &m) const
	{
		tvector2 r;
		r.x = x * m;
		r.y = y * m;
		return r;
	}
	tvector2 operator/(const T &m) const
	{
		tvector2 r;
		r.x = x / m;
		r.y = y / m;
		return r;
	}

	// Unary operators
	tvector2 operator+() const
	{
		tvector2 r;
		r.x = +x;
		r.y = +y;
		return r;
	}
	tvector2 operator-() const
	{
		tvector2 r;
		r.x = -x;
		r.y = -y;
		return r;
	}

	// Left hand arithmetics operators
	friend tvector2 operator*(const T &m, const tvector2 &v)
	{
		tvector2 r;
		r = v * m;
		return r;
	}
	friend tvector2 operator/(const T &m, const tvector2 &v)
	{
		tvector2 r;
		r.x = m / v.x;
		r.y = m / v.y;
		return r;
	}
};

typedef tvector2<float> vec2;
typedef tvector2<double> vec2d;
typedef tvector2<int> int2;
typedef tvector2<unsigned int> uint2;

// POD-compatible packed version for use in packed structs
template <typename T>
struct pvector2
{
	T x, y;
};

typedef pvector2<float> vec2_packed;
typedef pvector2<double> vec2d_packed;
typedef pvector2<int> int2_packed;
typedef pvector2<unsigned int> uint2_packed;

#endif