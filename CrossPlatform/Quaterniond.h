#ifndef QuaterniondH
#define QuaterniondH

#include "Platform/Math/Matrix.h"    
#include "Platform/Shaders/SL/CppSl.sl"
#include "Export.h"
#include <algorithm>
#include <math.h>

namespace platform
{
	namespace crossplatform
	{
		/// Quaternion template class to represent rotations.
		template<typename T> class Quaternion
		{
		public:
			T x,y,z,s;
			class BadParameter{};
			// Constructors
			Quaternion()
			{
				Reset();
			}
			Quaternion(const tvector3<T>& v, T S)
			{
				x = v.x;
				y = v.y;
				z = v.z;
				s = S;
			}
			Quaternion(const tvector4<T> &v)
			{
				x=v.x;
				y=v.y;
				z=v.z;
				s=v.w;
			}
			Quaternion(T X,T Y,T Z,T S,bool normalize=true)
			{
				x = X;
				y = Y;
				z = Z;
				s = S;
				if (normalize)
					this->MakeUnit();
			}

			Quaternion(const T *q)
			{
				x = q[0];
				y = q[1];
				z = q[2];
				s = q[3];
			}
			template<typename U> Quaternion(const Quaternion<U> &q)
			{
				s = T(q.s);
				x = T(q.x);
				y = T(q.y);
				z = T(q.z);
			}

			Quaternion(T angle_radians, const tvector3<T>& vv)
			{
				Define(angle_radians, vv);
			}
			template<typename U> operator tvector4<U>() const
			{
				tvector4<U> v;
				v.x = (U)x;
				v.y = (U)y;
				v.z = (U)z;
				v.w = (U)s;
				return v;
			}
			operator tvector4<T>() const
			{
				tvector4<T> v;
				v.x = x;
				v.y = y;
				v.z = z;
				v.w = s;
				return v;
			}
			void Reset()
			{
				x = (T)0.0;
				y = (T)0.0;
				z = (T)0.0;
				s = (T)1.0;
			}
			template<typename U> void Define(T angle, const tvector3<U>& vv)
			{
				angle /= T(2.0);
				s = cos(angle);
				T ss = sin(angle);
				x = ss * vv.x;
				y = ss * vv.y;
				z = ss * vv.z;
			}
			template<typename U> void Define(const tvector3<U>& dir_sin)
			{
				T mag = T(length(dir_sin));
				T a2 = asin(mag) / T(2.0);
				s = cos(a2);
				T div = T(1.0) / (T(2.0) * cos(a2));
				x = dir_sin.x * div;
				y = dir_sin.y * div;
				z = dir_sin.z * div;
			}
			void Define(const T ss,const T xx,const T yy,const T zz)
			{
				s = ss;
				x = xx;
				y = yy;
				z = zz;
			}
			Quaternion operator*(const Quaternion &q) const
			{
				Quaternion r;
				r.s = s * q.s - x * q.x - y * q.y - z * q.z;
				r.x = s * q.x + x * q.s + y * q.z - z * q.y;
				r.y = s * q.y + y * q.s + z * q.x - x * q.z;
				r.z = s * q.z + z * q.s + x * q.y - y * q.x;
				return r;
			}
			Quaternion operator/(const Quaternion &q) const
			{
				Quaternion iq = q;
				iq.x *= (T)-1.0;
				iq.y *= (T)-1.0;
				iq.z *= (T)-1.0;
				return (*this) * iq;
			}
			static Quaternion Invalid()
			{
				Quaternion q;
				q.x=q.y=q.z=q.s=0.0f;
				return q;
			}
			bool IsValid() const
			{
				return (x!=0.0f||y!=0.0f||z!=0.0f)||(s!=0.0f);
			}
			void MakeUnit()
			{
				T magnitude;
				magnitude = (T)sqrt((x * x + y * y + z * z + s * s));
				if (magnitude == 0)
				{
					s = (T)1.0;
					return;
				}
				x /= magnitude;
				y /= magnitude;
				z /= magnitude;
				s /= magnitude;
			}
			T AngleInDirection(const tvector3<T> &vv) const
			{
				T dp = (T)dot(vv, tvector3<T>(x, y, z));
				T halfangle = (T) asin(dp);
				return halfangle * (T)2.0;
			}
			T Angle() const
			{
				T halfangle = acos(std::min((T)1.0, std::max((T)-1.0, s)));
				return halfangle * (T)2.0;
			}
			// Operations
			operator const T*() const
			{
				return (const T *)(&x);
			}
			tvector3<T> operator*(const tvector3<T>&vec) const
			{
				const T &x0 = vec.x;
				const T &y0 = vec.y;
				const T &z0 = vec.z;
				T s1 = x * x0 + y * y0 + z * z0;
				T x1 = s * x0 + y * z0 - z * y0;
				T y1 = s * y0 + z * x0 - x * z0;
				T z1 = s * z0 + x * y0 - y * x0;
				return tvector3<T>(s1 * x + s * x1 + y * z1 - z * y1,
					s1 * y + s * y1 + z * x1 - x * z1,
					s1 * z + s * z1 + x * y1 - y * x1);
			}

			/// Equivalent to (!this)* vec
			tvector3<T> operator/(const tvector3<T>&vec) const
			{
				const T &x0 = vec.x;
				const T &y0 = vec.y;
				const T &z0 = vec.z;
				T s1 = -x * x0 - y * y0 - z * z0;
				T x1 = s * x0 - y * z0 + z * y0;
				T y1 = s * y0 - z * x0 + x * z0;
				T z1 = s * z0 - x * y0 + y * x0;
				return tvector3<T>(-s1 * x + s * x1 - y * z1 + z * y1,
					-s1 * y + s * y1 - z * x1 + x * z1,
					-s1 * z + s * z1 - x * y1 + y * x1);
			}

			Quaternion operator*(T d)
			{
				return Quaternion(x,y,z,s*d,false);
			}
			/// The inverse, or opposite of the Quaternion, representing an equal rotation
			/// in the opposite direction.
			Quaternion operator!() const
			{
				Quaternion r;
				r.s=s;
				r.x=-x;
				r.y=-y;
				r.z=-z;
				return r;
			}
			Quaternion& operator=(const T *q)
			{
				x=q[0];
				y=q[1];
				z=q[2];
				s=q[3];
				return *this;
			}
			Quaternion& operator=(const tvector4<T> &v)
			{
				x=v.x;
				y=v.y;
				z=v.z;
				s=v.w;
				return *this;
			}
			bool operator==(const Quaternion &q)
			{
				return(s==q.s&&x==q.x&&y==q.y&&z==q.z);
			}
			bool operator!=(const Quaternion &q)
			{
				return(s!=q.s||x!=q.x||y!=q.y||z!=q.z);
			}
			/// Assignment operator. Set this Quaternion equal to q.
			Quaternion& operator=(const Quaternion &q)
			{
				x=q.x;
				y=q.y;
				z=q.z;
				s=q.s;
				return *this;
			}
			//------------------------------------------------------------------------------
			// (X,Y,Z,s) => s*q + q.s*this
			//			 +  (z,x,y,0)*q.(y,z,x,0) - (y,z,x,0)*q.(z,x,y,0)
			//			s -= this*q
			Quaternion& operator/=(const Quaternion &q)
			{
				T X, Y, Z;
				X = s * q.x + q.s * x + q.y * z - q.z * y;
				Y = s * q.y + q.s * y + q.z * x - q.x * z;
				Z = s * q.z + q.s * z + q.x * y - q.y * x;
				s = q.s * s - q.x * x - q.y * y - q.z * z;

				x = X;
				y = Y;
				z = Z;
				return *this;
			}
			//Quaternion& operator*=(double d)
			//{
			//	Quaternion q(s*d,x,y,z);
			//	*this=q;
			//    return *this;
			//}
			Quaternion& Rotate(T angle,const tvector3<T>&axis)
			{
				Quaternion dq(angle, axis);
				*this = dq * (*this);
				MakeUnit();
				return *this;
			}
			void Rotate(const tvector3<T>&d)
			{
				double sz = length(d);
				if (sz > 0)
				{
					tvector3<T> a = d;
					a /= sz;
					Quaternion dq(sz, a);
					T s1 = dq.s * s - dq.x * x - dq.y * y - dq.z * z;
					T x1 = s * dq.x + dq.s * x + dq.y * z - dq.z * y;
					T y1 = s * dq.y + dq.s * y + dq.z * x - dq.x * z;
					T z1 = s * dq.z + dq.s * z + dq.x * y - dq.y * x;
					s = s1;
					x = x1;
					y = y1;
					z = z1;
				}
			}
			template<typename U> void DefineSmall(T ss, const tvector3<U>& vv)
			{
				ss *= T(0.5);
				x = ss * T(vv.x);
				y = ss * T(vv.y);
				z = ss * T(vv.z);
				s = T(1.0);
				T magnitude;
				magnitude = sqrt((x * x + y * y + z * z + s * s));
				if (magnitude == 0)
					return;
				x /= magnitude;
				y /= magnitude;
				z /= magnitude;
				s /= magnitude;
			}
		};
		template<typename T> void Multiply(Quaternion<T>& r, const Quaternion<T>& q1, const Quaternion<T>& q2)
		{
			r.s = q1.s * q2.s - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
			r.x = q2.s * q1.x + q1.s * q2.x + q1.y * q2.z - q1.z * q2.y;
			r.y = q2.s * q1.y + q1.s * q2.y + q1.z * q2.x - q1.x * q2.z;
			r.z = q2.s * q1.z + q1.s * q2.z + q1.x * q2.y - q1.y * q2.x;
		}
		template<typename T> void Multiply(tvector3<T>& ret, const Quaternion<T>& q, const tvector3<T>& v)
		{
			const T& x0 = v.x;
			const T& y0 = v.y;
			const T& z0 = v.z;
			T s1 = q.x * x0 + q.y * y0 + q.z * z0;
			T x1 = q.s * x0 + q.y * z0 - q.z * y0;
			T y1 = q.s * y0 + q.z * x0 - q.x * z0;
			T z1 = q.s * z0 + q.x * y0 - q.y * x0;
			ret.x = s1 * q.x + q.s * x1 + q.y * z1 - q.z * y1;
			ret.y = s1 * q.y + q.s * y1 + q.z * x1 - q.x * z1;
			ret.z = s1 * q.z + q.s * z1 + q.x * y1 - q.y * x1;
		}
		typedef Quaternion<double> Quaterniond;
		typedef Quaternion<float> Quaternionf;
		/// Multiply, or rotate, vec3d v by q, return the value in vec3d ret. v and ret must have size 3.
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN Multiply(vec3d & ret,const Quaterniond & q,const vec3d & v);
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN MultiplyByNegative(Quaterniond& ret,const Quaterniond& q1,const Quaterniond& q2);
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN MultiplyNegativeByQuaterniond(Quaterniond& r,const Quaterniond& q1,const Quaterniond& q2);
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN Divide(vec3d& ret,const Quaterniond& q,const vec3d& v);
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN AddQuaterniondTimesVector(vec3d& ret,const Quaterniond& q,const vec3d& v);
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN Multiply(Quaterniond& r,const Quaterniond& q1,const Quaterniond& q2);
	
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN Slerp(Quaterniond &ret,const Quaterniond &q1,const Quaterniond &q2,double l);  
		extern double SIMUL_CROSSPLATFORM_EXPORT_FN angleBetweenQuaternions(const crossplatform::Quaterniond& q1, const crossplatform::Quaterniond& q2);
		extern Quaterniond SIMUL_CROSSPLATFORM_EXPORT_FN rotateByOffsetCartesian(const Quaterniond& input, const vec2& offset, float sph_radius);
		extern Quaterniond SIMUL_CROSSPLATFORM_EXPORT_FN rotateByOffsetPolar(const Quaterniond& input, float polar_radius, float polar_angle, float sph_radius);

		extern Quaterniond SIMUL_CROSSPLATFORM_EXPORT_FN LocalToGlobalOrientation(const Quaterniond& origin,const Quaterniond& local) ;
		extern Quaterniond SIMUL_CROSSPLATFORM_EXPORT_FN GlobalToLocalOrientation(const Quaterniond& origin,const Quaterniond& global) ;
		
		//! Transform a position in a previous frame of reference into a new frame. Assumes Earth radius 6378km, origin at sea level.
		extern vec3 SIMUL_CROSSPLATFORM_EXPORT_FN TransformPosition(Quaterniond old_origin,Quaterniond new_origin,vec3 old_pos, double radius= 6378000.0);
		//! Rotate an orientation by a specified offset in its local x and y axes.
		extern Quaterniond SIMUL_CROSSPLATFORM_EXPORT_FN TransformOrientationByOffsetXY(const Quaterniond &origin,vec2 local_offset_radians);
		template<typename T,typename U> void QuaternionToMatrix(tmatrix4<T>& M, const Quaternion<U>& q)
		{
			T X = (T)q.x;
			T Y = (T)q.y;
			T Z = (T)q.z;
			T S = (T)q.s;
			T sqw = S * S;
			T sqx = X * X;
			T sqy = Y * Y;
			T sqz = Z * Z;
			T invs = T(1.0) / (sqx + sqy + sqz + sqw);
			M.m[0] = (T)((sqx - sqy - sqz + sqw) * invs);
			M.m[1] = (T)(T(2.0) * (X * Y + Z * S));      
			M.m[2] = (T)(T(2.0) * (X * Z - Y * S));      
			 
			M.m[4 + 0] = (T)(T(2.0) * (X * Y - Z * S));      
			M.m[4 + 1] = (T)(-X * X + Y * Y - Z * Z + S * S);
			M.m[4 + 2] = (T)(T(2.0) * (Y * Z + X * S));      
			
			M.m[2 * 4 + 0] = (T)(T(2.0) * (X * Z + Y * S));     
			M.m[2 * 4 + 1] = (T)(T(2.0) * (Y * Z - X * S));     
			M.m[2 * 4 + 2] = (T)(-X * X - Y * Y + Z * Z + S * S);
		}
		template<typename T, typename U> void MatrixToQuaternion(Quaternion<T>& q, const tmatrix4<U>& M)
		{
			tmatrix4<U> Mt = M;
			Mt.transpose();
			//    Calculate the trace of the matrix Mt from the equation:
			T Tr = (T)Mt.M[0][0] + (T)Mt.M[1][1] + (T)Mt.M[2][2] + T(1.0);
			//    If the trace of the matrix is greater than zero
			if (Tr > 0)
			{
				T Size = T(2.0) * (T)sqrt(Tr);
				q.s = (T)0.25 * Size;
				q.x = ((T)(Mt._m21 - Mt._m12) / Size);
				q.y = ((T)(Mt._m02 - Mt._m20) / Size);
				q.z = ((T)(Mt._m10 - Mt._m01) / Size);
			}
			else
			{
				T r[4];
				int i, j, k;
				int nxt[3] = { 1, 2, 0 };
				// diagonal is negative
				i = 0;
				if ((T)Mt.M[1][1] > (T)Mt.M[0][0])
					i = 1;
				if ((T)Mt.M[2][2] > (T)Mt.M[i][i])
					i = 2;
				j = nxt[i];
				k = nxt[j];
				T Size = sqrt(((T)Mt.M[i][i] - ((T)Mt.M[j][j] + (T)Mt.M[k][k])) + T(1.0));
				r[i] = Size * T(0.5);
				if (Size != T(0.0))
					Size = T(0.5) / Size;
				r[j] = ((T)Mt.M[i][j] + (T)Mt.M[j][i]) * Size;
				r[k] = ((T)Mt.M[i][k] + (T)Mt.M[k][i]) * Size;
				r[3] = ((T)Mt.M[j][k] - (T)Mt.M[k][j]) * Size;

				q.x = -r[0];
				q.y = -r[1];
				q.z = -r[2];
				q.s = r[3];
			}
			q.MakeUnit();
		}
		
	}
}
#endif
