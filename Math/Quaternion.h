#ifndef QuaternionH
#define QuaternionH

#include "Platform/Math/Matrix.h"    
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/Vector3.h"
#include "Export.h"

namespace simul
{
	namespace math
	{
		extern void SIMUL_MATH_EXPORT_FN Slerp(Quaternion &ret,const Quaternion &q1,const Quaternion &q2,float l);  
		/// Quaternion class to represent rotations.
		class SIMUL_MATH_EXPORT Quaternion
		{
		public:
			float x,y,z,s;
			class BadParameter{};
			// Constructors
			Quaternion();    
			Quaternion(float s,float x,float y,float z,bool normalize);
			Quaternion(const float *q);
			Quaternion(const Quaternion &q);
			Quaternion(float angle_radians,const Vector3 &vv);
			void Reset();
			void Define(float angle,const Vector3 &vv);
			void Define(const Vector3 &dir_sin);
			void DefineSmall(float angle,const Vector3 &vv);
			void Define(const float ss,const float xx,const float yy,const float zz);
			Quaternion operator*(const Quaternion &q) const;
			Quaternion operator/(const Quaternion &q) const;
			void MakeUnit();
			float AngleInDirection(const Vector3 &vv) const;
			float Angle() const;  
			/// Multiply, or rotate, Vector3 v by q, return the value in Vector3 ret. v and ret must have size 3.
			friend void SIMUL_MATH_EXPORT_FN Multiply(Vector3 &ret,const Quaternion &q,const Vector3 &v);    
			friend void SIMUL_MATH_EXPORT_FN MultiplyByNegative(Quaternion &ret,const Quaternion &q1,const Quaternion &q2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyNegativeByQuaternion(Quaternion &r,const Quaternion &q1,const Quaternion &q2);
			friend void SIMUL_MATH_EXPORT_FN Divide(Vector3 &ret,const Quaternion &q,const Vector3 &v);
			friend void SIMUL_MATH_EXPORT_FN AddQuaternionTimesVector(Vector3 &ret,const Quaternion &q,const Vector3 &v);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Quaternion &r,const Quaternion &q1,const Quaternion &q2);
			// Operations
			operator const float*() const
			{
				return (const float *)(&x);
			}
			Vector3 operator*(const Vector3 &vec) const
			{
				const float x0=vec(0);
				const float y0=vec(1);
				const float z0=vec(2);
				float s1=x*x0+y*y0+z*z0;
				float x1=s*x0+y*z0-z*y0;
				float y1=s*y0+z*x0-x*z0;
				float z1=s*z0+x*y0-y*x0;
				return Vector3(s1*x+s*x1+y*z1-z*y1,
							s1*y+s*y1+z*x1-x*z1,
							s1*z+s*z1+x*y1-y*x1);
			}
			Vector3 operator/(const Vector3 &vec) const
			{
				const float x0=vec(0);
				const float y0=vec(1);
				const float z0=vec(2);
				float s1=-x*x0-y*y0-z*z0;
				float x1= s*x0-y*z0+z*y0;
				float y1= s*y0-z*x0+x*z0;
				float z1= s*z0-x*y0+y*x0;
				return Vector3(-s1*x+s*x1-y*z1+z*y1,
							-s1*y+s*y1-z*x1+x*z1,
							-s1*z+s*z1-x*y1+y*x1);
			}
			Quaternion operator*(float d)
			{
				return Quaternion(s*d,x,y,z,false);
			}
			/// The inverse, or opposite of the Quaternion, representing an equal rotation
			/// in the opposite direction.
			Quaternion operator!()
			{
				Quaternion r;
				r.s=s;
				r.x=-x;
				r.y=-y;
				r.z=-z;
				return r;
			}
			Quaternion& operator=(const float *q)
			{
				x=q[0];
				y=q[1];
				z=q[2];
				s=q[3];
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
			#ifndef PLAYSTATION_2
				x=q.x;
				y=q.y;
				z=q.z;
				s=q.s;
			#else
				const float *Q2=&(q.x);
				asm __volatile__("  
						.set noreorder
					lqc2			vf1,0(%1)			// Load q's float address into vf1
					sqc2			vf1,0(%0)			// Load vf1 into r's float address
					"					: /* Outputs. */
										: /* Inputs */ "r" (this), "r" (Q2)
										: /* Clobber */ "$vf1");
			#endif
				return *this;
			}
			//------------------------------------------------------------------------------
			// (X,Y,Z,s) => s*q + q.s*this
			//			 +  (z,x,y,0)*q.(y,z,x,0) - (y,z,x,0)*q.(z,x,y,0)
			//			s -= this*q
			Quaternion& operator/=(const Quaternion &q);
			//Quaternion& operator*=(float d)
			//{
			//	Quaternion q(s*d,x,y,z);
			//	*this=q;
			//    return *this;
			//}
			Quaternion& Rotate(float angle,const Vector3 &axis)
			{
				Quaternion dq(angle,axis);
				*this=dq*(*this);
				MakeUnit();
				return *this;
			}         
			void Rotate(const Vector3 &d)
			{
				float size=d.Magnitude();
				if(size>0)
				{
					Vector3 a=d;
					a/=size;
					Quaternion dq(size,a);
					float s1=dq.s*s-dq.x*x-dq.y*y-dq.z*z;
					float x1=s*dq.x+dq.s*x+dq.y*z-dq.z*y;
					float y1=s*dq.y+dq.s*y+dq.z*x-dq.x*z;
					float z1=s*dq.z+dq.s*z+dq.x*y-dq.y*x;
					s=s1;
					x=x1;
					y=y1;
					z=z1;
				}
			}
			// Friend functions
			friend void SIMUL_MATH_EXPORT_FN FPUQuaternionToMatrix(Matrix &m,const Quaternion &q);
			/// Assign Matrix m as the rotation matrix equivalent to Quaternion q. m must be 3 by 3.
			friend void SIMUL_MATH_EXPORT_FN QuaternionToMatrix(Matrix4x4 &m,const Quaternion &q);       
			/// Assign q to be the Quaternion equivalent to rotation Matrix M. M must be 3 by 3.
			friend void SIMUL_MATH_EXPORT_FN MatrixToQuaternion(Quaternion &q,const Matrix4x4 &M);
			friend Quaternion SIMUL_MATH_EXPORT_FN operator*(const float d,const Quaternion &q);
		};        
	extern void SIMUL_MATH_EXPORT_FN MatrixToQuaternion(Quaternion &q,const Matrix4x4 &M);
}
}
#endif
