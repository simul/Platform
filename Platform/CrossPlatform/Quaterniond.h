#ifndef QuaterniondH
#define QuaterniondH

#include "Simul/Math/Matrix.h"    
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Export.h"

namespace simul
{
	namespace crossplatform
	{
		/// Quaterniond class to represent rotations.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS Quaterniond
		{
		public:
			double x,y,z,s;
			class BadParameter{};
			// Constructors
			Quaterniond();    
			Quaterniond(double s,double x,double y,double z,bool normalize=true);
			Quaterniond(const double *q);
			Quaterniond(const Quaterniond &q);
			Quaterniond(double angle_radians,const vec3d &vv);
			void Reset();
			void Define(double angle,const vec3d &vv);
			void Define(const vec3d &dir_sin);
			void DefineSmall(double angle,const vec3d &vv);
			void Define(const double ss,const double xx,const double yy,const double zz);
			Quaterniond operator*(const Quaterniond &q) const;
			Quaterniond operator/(const Quaterniond &q) const;
			static Quaterniond Invalid()
			{
				Quaterniond q;
				q.x=q.y=q.z=q.s=0.0f;
				return q;
			}
			bool IsValid() const
			{
				return (x!=0.0f||y!=0.0f||z!=0.0f)||(s!=0.0f);
			}
			void MakeUnit();
			double AngleInDirection(const vec3d &vv) const;
			double Angle() const;  
			/// Multiply, or rotate, vec3d v by q, return the value in vec3d ret. v and ret must have size 3.
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN Multiply(vec3d &ret,const Quaterniond &q,const vec3d &v);    
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN MultiplyByNegative(Quaterniond &ret,const Quaterniond &q1,const Quaterniond &q2);
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN MultiplyNegativeByQuaterniond(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2);
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN Divide(vec3d &ret,const Quaterniond &q,const vec3d &v);
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN AddQuaterniondTimesVector(vec3d &ret,const Quaterniond &q,const vec3d &v);
			friend void SIMUL_CROSSPLATFORM_EXPORT_FN Multiply(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2);
			// Operations
			operator const double*() const
			{
				return (const double *)(&x);
			}
			vec3d operator*(const vec3d &vec) const;
			vec3d operator/(const vec3d &vec) const;
			Quaterniond operator*(double d)
			{
				return Quaterniond(s*d,x,y,z,false);
			}
			/// The inverse, or opposite of the Quaterniond, representing an equal rotation
			/// in the opposite direction.
			Quaterniond operator!()
			{
				Quaterniond r;
				r.s=s;
				r.x=-x;
				r.y=-y;
				r.z=-z;
				return r;
			}
			Quaterniond& operator=(const double *q)
			{
				x=q[0];
				y=q[1];
				z=q[2];
				s=q[3];
				return *this;
			}
			bool operator==(const Quaterniond &q)
			{
				return(s==q.s&&x==q.x&&y==q.y&&z==q.z);
			}
			bool operator!=(const Quaterniond &q)
			{
				return(s!=q.s||x!=q.x||y!=q.y||z!=q.z);
			}
			/// Assignment operator. Set this Quaterniond equal to q.
			Quaterniond& operator=(const Quaterniond &q)
			{
			#ifndef PLAYSTATION_2
				x=q.x;
				y=q.y;
				z=q.z;
				s=q.s;
			#else
				const double *Q2=&(q.x);
				asm __volatile__("  
						.set noreorder
					lqc2			vf1,0(%1)			// Load q's double address into vf1
					sqc2			vf1,0(%0)			// Load vf1 into r's double address
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
			Quaterniond& operator/=(const Quaterniond &q);
			//Quaterniond& operator*=(double d)
			//{
			//	Quaterniond q(s*d,x,y,z);
			//	*this=q;
			//    return *this;
			//}
			Quaterniond& Rotate(double angle,const vec3d &axis);
			void Rotate(const vec3d &d);
		};
		extern void SIMUL_CROSSPLATFORM_EXPORT_FN Slerp(Quaterniond &ret,const Quaterniond &q1,const Quaterniond &q2,double l);  
	}
}
#endif
