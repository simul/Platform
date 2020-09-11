#ifndef QuaterniondH
#define QuaterniondH

#include "Platform/Math/Matrix.h"    
#include "Platform/Shaders/SL/CppSl.sl"
#include "Export.h"
#include <algorithm>
#include <math.h>

namespace simul
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
			Quaternion(const Quaternion &q)
			{
				s = q.s;
				x = q.x;
				y = q.y;
				z = q.z;
			}

			Quaternion(T angle_radians, const tvector3<T>& vv)
			{
				Define(angle_radians, vv);
			}
			operator vec4() const
			{
				vec4 v;
				v.x = (float)x;
				v.y = (float)y;
				v.z = (float)z;
				v.w = (float)s;
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
				magnitude = sqrt((x * x + y * y + z * z + s * s));
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
			operator const double*() const
			{
				return (const double *)(&x);
			}
			vec3d operator*(const tvector3<T>&vec) const
			{
				const T x0 = vec.x;
				const T y0 = vec.y;
				const T z0 = vec.z;
				T s1 = x * x0 + y * y0 + z * z0;
				T x1 = s * x0 + y * z0 - z * y0;
				T y1 = s * y0 + z * x0 - x * z0;
				T z1 = s * z0 + x * y0 - y * x0;
				return tvector3<T>(s1 * x + s * x1 + y * z1 - z * y1,
					s1 * y + s * y1 + z * x1 - x * z1,
					s1 * z + s * z1 + x * y1 - y * x1);
			}

			/// Equivalent to (!this)* vec
			vec3d operator/(const tvector3<T>&vec) const
			{
				const T x0 = vec.x;
				const T y0 = vec.y;
				const T z0 = vec.z;
				T s1 = -x * x0 - y * y0 - z * z0;
				T x1 = s * x0 - y * z0 + z * y0;
				T y1 = s * y0 - z * x0 + x * z0;
				T z1 = s * z0 - x * y0 + y * x0;
				return tvector3<T>(-s1 * x + s * x1 - y * z1 + z * y1,
					-s1 * y + s * y1 - z * x1 + x * z1,
					-s1 * z + s * z1 - x * y1 + y * x1);
			}

			Quaternion operator*(double d)
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
			Quaternion& operator=(const double *q)
			{
				x=q[0];
				y=q[1];
				z=q[2];
				s=q[3];
				return *this;
			}
			Quaternion& operator=(const vec4 &v)
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
				double X, Y, Z;
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
					double s1 = dq.s * s - dq.x * x - dq.y * y - dq.z * z;
					double x1 = s * dq.x + dq.s * x + dq.y * z - dq.z * y;
					double y1 = s * dq.y + dq.s * y + dq.z * x - dq.x * z;
					double z1 = s * dq.z + dq.s * z + dq.x * y - dq.y * x;
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
		
	}
}
#endif
