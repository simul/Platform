
#define SIM_MATH
#include "Quaterniond.h"
#include "Platform/Core/RuntimeError.h"

#include <math.h>
#include <algorithm>

using namespace simul;
using namespace crossplatform;

 double length(vec3d v)
{
	return sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
}

double dot(vec3d& a,vec3d& b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}
namespace simul
{
	namespace crossplatform
	{
		void Multiply(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2)
		{
			r.s=q1.s*q2.s-q1.x*q2.x-q1.y*q2.y-q1.z*q2.z;
			r.x=q2.s*q1.x+q1.s*q2.x+q1.y*q2.z-q1.z*q2.y;
			r.y=q2.s*q1.y+q1.s*q2.y+q1.z*q2.x-q1.x*q2.z;
			r.z=q2.s*q1.z+q1.s*q2.z+q1.x*q2.y-q1.y*q2.x;
		}           
		//------------------------------------------------------------------------------
		void MultiplyNegativeByQuaternion(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2)
		{
			r.s=q1.s*q2.s +q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			r.x=-q2.s*q1.x+q1.s*q2.x-q1.y*q2.z+q1.z*q2.y;
			r.y=-q2.s*q1.y+q1.s*q2.y-q1.z*q2.x+q1.x*q2.z;
			r.z=-q2.s*q1.z+q1.s*q2.z-q1.x*q2.y+q1.y*q2.x;
		}
		void MultiplyByNegative(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2)
		{
			r.s=q1.s*q2.s+q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			r.x=q2.s*q1.x-q1.s*q2.x-q1.y*q2.z+q1.z*q2.y;
			r.y=q2.s*q1.y-q1.s*q2.y-q1.z*q2.x+q1.x*q2.z;
			r.z=q2.s*q1.z-q1.s*q2.z-q1.x*q2.y+q1.y*q2.x;
		}
		// v must be a 3-vector, ret must be a 3-vector.
		void Multiply(vec3d &ret,const Quaterniond &q,const vec3d &v)
		{
			const double &x0=v.x;
			const double &y0=v.y;
			const double &z0=v.z;
			double s1=q.x*x0+q.y*y0+q.z*z0;
			double x1=q.s*x0+q.y*z0-q.z*y0;
			double y1=q.s*y0+q.z*x0-q.x*z0;
			double z1=q.s*z0+q.x*y0-q.y*x0;
			ret.x=s1*q.x+q.s*x1+q.y*z1-q.z*y1;
			ret.y=s1*q.y+q.s*y1+q.z*x1-q.x*z1;
			ret.z=s1*q.z+q.s*z1+q.x*y1-q.y*x1;
		}             
		// v must be a 3-vector, ret must be a 3-vector.
		void AddQuaternionTimesVector(vec3d &ret,const Quaterniond &q,const vec3d &v)
		{
			const double &x0=v.x;
			const double &y0=v.y;
			const double &z0=v.z;
			double s1=q.x*x0+q.y*y0+q.z*z0;
			double x1=q.s*x0+q.y*z0-q.z*y0;
			double y1=q.s*y0+q.z*x0-q.x*z0;
			double z1=q.s*z0+q.x*y0-q.y*x0;
			ret.x+=s1*q.x+q.s*x1+q.y*z1-q.z*y1;
			ret.y+=s1*q.y+q.s*y1+q.z*x1-q.x*z1;
			ret.z+=s1*q.z+q.s*z1+q.x*y1-q.y*x1;
		}
		void Divide(vec3d &ret,const Quaterniond &q,const vec3d &v)
		{
			const double &x0=v.x;
			const double &y0=v.y;
			const double &z0=v.z;
			double s1;
			s1=-q.x*x0-q.y*y0-q.z*z0;
			double x1;
			x1= q.s*x0-q.y*z0+q.z*y0;
			double y1;
			y1= q.s*y0-q.z*x0+q.x*z0;
			double z1;
			z1= q.s*z0-q.x*y0+q.y*x0;
			ret.x=-s1*q.x+q.s*x1-q.y*z1+q.z*y1,
			ret.y=-s1*q.y+q.s*y1-q.z*x1+q.x*z1,
			ret.z=-s1*q.z+q.s*z1-q.x*y1+q.y*x1;
		}

		void Slerp(Quaterniond &ret,const Quaterniond &q1,const Quaterniond &q2,double l)
		{
			// v0 and v1 should be unit length or else
			// something broken will happen.

			// Compute the cosine of the angle between the two vectors.
			Quaterniond Q2=q2;
			double dot = q1.x*q2.x+q1.y*q2.y+q1.z*q2.z + q1.s * q2.s;
			if(dot<0)
			{
				dot=-dot;
				Q2.x*=-1.0;
				Q2.y*=-1.0;
				Q2.z*=-1.0;
				Q2.s*=-1.0;
			}
			//dot+=q1.s*Q2.s;
			
			const double DOT_THRESHOLD = 0.9995;
			if (dot > DOT_THRESHOLD)
			{
				// If the inputs are too close for comfort, linearly interpolate
				// and normalize the result.
				ret.x=q1.x+l*(Q2.x - q1.x);
				ret.y=q1.y+l*(Q2.y-q1.y);
				ret.z=q1.z+l*(Q2.z-q1.z);
				ret.s=q1.s+l*(Q2.s-q1.s);
				ret.MakeUnit();
				return;
			}
			if(dot<-1.0)
				dot=-1.0;
			if(dot>1.0)
				dot=1.0;
			double theta_0 = acos(dot);  // theta_0 = half angle between input vectors
			double theta1 = theta_0*(1.0-l);    // theta = angle between v0 and result 
			double theta2 = theta_0*l;    // theta = angle between v0 and result 

			double s1=sin(theta1);
			double s2=sin(theta2);
			double ss=sin(theta_0);
			/*Quaterniond q3;
			q3.x=Q2.x-dot*(q1.x);
			q3.y=Q2.y-dot*(q1.y);
			q3.z=Q2.z-dot*(q1.z);
			q3.s=Q2.s-dot*(q1.s);
			q3.MakeUnit();              // { v0, v2 } is now an orthonormal basis*/

			ret.x=(q1.x*s1+Q2.x*s2)/ss;
			ret.y=(q1.y*s1+Q2.y*s2)/ss;
			ret.z=(q1.z*s1+Q2.z*s2)/ss;
			ret.s=(q1.s*s1+Q2.s*s2)/ss;
			ret.MakeUnit();          
		}

	}
}



double simul::crossplatform::angleBetweenQuaternions(const crossplatform::Quaterniond& q1, const crossplatform::Quaterniond& q2)
{
	crossplatform::Quaterniond Q1 = q1;
	crossplatform::Quaterniond Q2 = q2;
	Q1.MakeUnit();
	Q2.MakeUnit();
	crossplatform::Quaterniond z = Q1 * !Q2;

	/*double dot = Q1.x*Q2.x + Q1.y*Q2.y + Q1.z*Q2.z + Q1.s*Q2.s;

	if (dot < 0.0)
		dot = 0.0;
	if (dot > 1.0)
		dot = 1.0;*/

	return 2 * acos(z.s);  //Angle between input quaternions
}



Quaterniond simul::crossplatform::rotateByOffsetCartesian(const Quaterniond& input, const vec2& offset, float sph_radius)
{
	//Check there's an offset;
	if (offset.x == 0 || offset.y == 0)
		return input;

	//Check's if the sph_radius is valid
	if (sph_radius <= 0)
		return input;

	double x_rad = offset.x / sph_radius;
	double y_rad = offset.y / sph_radius;
	crossplatform::Quaterniond x_rot(x_rad, vec3d(0.0, 1.0, 0.0));
	crossplatform::Quaterniond y_rot(-y_rad, vec3d(1.0, 0.0, 0.0));

	return x_rot * y_rot * input;
}

Quaterniond simul::crossplatform::rotateByOffsetPolar(const Quaterniond& input, float polar_radius, float polar_angle, float sph_radius)
{	
	//Check there's an offset;
	if (polar_radius == 0 || polar_angle == 0)
		return input;

	//Check's if the sph_radius is valid
	if (sph_radius <= 0)
		return input;

	//Calculate angle offset from equatorial plane				
	double phi = polar_angle;							//double phi = atan2(offset.y, offset.x);
	//Calculate angle of the arclength from the offset 
	double theta = polar_radius / sph_radius;			//double theta = (double)(length(offset) / sph_radius);
	
	crossplatform::Quaterniond x_rot(theta, vec3d(0.0, 1.0, 0.0));
	crossplatform::Quaterniond z_rot(phi, vec3d(0.0, 0.0, 1.0));

	return z_rot * x_rot * input;
}


crossplatform::Quaterniond simul::crossplatform::LocalToGlobalOrientation(const crossplatform::Quaterniond& origin,const crossplatform::Quaterniond& local)
{
	// rel is a rotation in local space. Convert it first to global space:
	crossplatform::Quaterniond new_relative = (origin * (local / origin));
	// And now we rotate to the new origin:
	crossplatform::Quaterniond new_quat = new_relative * origin;
	return new_quat;
}
crossplatform::Quaterniond simul::crossplatform::GlobalToLocalOrientation(const crossplatform::Quaterniond& origin,const crossplatform::Quaterniond& global)
{
	// abso is an orientation in global space. First get the global rotation between the keyframe's origin and abso:
	crossplatform::Quaterniond dq = global/ origin;
	// Then convert this into the space of the keyframe:
	crossplatform::Quaterniond rel = ((!origin) * dq) * origin;
	return rel;
}

crossplatform::Quaterniond simul::crossplatform::TransformOrientationByOffsetXY(const crossplatform::Quaterniond &origin,vec2 local_offset_radians)
{
		// Now the new quaternion is 
		crossplatform::Quaterniond local_offset;
		local_offset.Rotate(local_offset_radians.x,vec3d(0,1.0,0));
		local_offset.Rotate(local_offset_radians.y,vec3d(-1.0,0,0));
		// local_offset must now be transformed to a global rotation.
		crossplatform::Quaterniond new_relative=(origin*(local_offset/origin));
		// And now we rotate to the new origin:
		crossplatform::Quaterniond new_quat=new_relative*origin;
		return new_quat;
}

vec3 simul::crossplatform::TransformPosition(Quaterniond old_origin,Quaterniond new_origin,vec3 old_pos,double radius)
{
	vec3d posd(old_pos.x,old_pos.y,radius+old_pos.z);
	vec3d globalposd;
	Multiply(globalposd,old_origin,posd);
	vec3d new_posd;
	Divide(new_posd,new_origin,globalposd);

	vec3 newpos((float)new_posd.x,(float)new_posd.y,(float)(new_posd.z-radius));
	return newpos;
}