#define NOMINMAX
#define SIM_MATH
#include "Quaterniond.h"
#include "Simul/Base/RuntimeError.h"

#include <math.h>
#include <algorithm>

using namespace simul;
using namespace crossplatform;

 double length(vec3d v)
{
	return sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
}

Quaterniond::Quaterniond()
{
	Reset();
}
Quaterniond::Quaterniond(double X,double Y,double Z,double S,bool normalize)
{
	x=X;
	y=Y;
	z=Z;
	s=S;
	if(normalize)
		this->MakeUnit();
}

Quaterniond::Quaterniond(const Quaterniond &q)
{
	s=q.s;
	x=q.x;
	y=q.y;
	z=q.z;
}

Quaterniond::Quaterniond(double angle,const vec3d &vv)
{
	Define(angle,vv);
}

Quaterniond::Quaterniond(const double *q)
{
	x=q[0];
	y=q[1];
	z=q[2];
	s=q[3];
}

Quaterniond::operator vec4() const
{
	vec4 v;
	v.x=(float)x;
	v.y=(float)y;
	v.z=(float)z;
	v.w=(float)s;
	return v;
}

vec3d Quaterniond::operator*(const vec3d &vec) const
{
	const double x0=vec.x;
	const double y0=vec.y;
	const double z0=vec.z;
	double s1=x*x0+y*y0+z*z0;
	double x1=s*x0+y*z0-z*y0;
	double y1=s*y0+z*x0-x*z0;
	double z1=s*z0+x*y0-y*x0;
	return vec3d(s1*x+s*x1+y*z1-z*y1,
				 s1*y+s*y1+z*x1-x*z1,
				 s1*z+s*z1+x*y1-y*x1);
}
vec3d Quaterniond::operator/(const vec3d &vec) const
{
	const double x0=vec.x;
	const double y0=vec.y;
	const double z0=vec.z;
	double s1=-x*x0-y*y0-z*z0;
	double x1= s*x0-y*z0+z*y0;
	double y1= s*y0-z*x0+x*z0;
	double z1= s*z0-x*y0+y*x0;
	return vec3d(-s1*x+s*x1-y*z1+z*y1,
				 -s1*y+s*y1-z*x1+x*z1,
				 -s1*z+s*z1-x*y1+y*x1);
}
void Quaterniond::Reset()
{
	x=0.0;
	y=0.0;
	z=0.0;
	s=1.0;
}

void Quaterniond::MakeUnit()
{
	double magnitude;
	magnitude=sqrt((x*x+y*y+z*z+s*s));
	if(magnitude==0)
	{
		s=1.0;
		return;
	}
	x/=magnitude;
	y/=magnitude;
	z/=magnitude;
	s/=magnitude;
}

void Quaterniond::DefineSmall(double ss,const vec3d &vv)
{
	ss*=0.5;
	x=ss*vv.x;
	y=ss*vv.y;
	z=ss*vv.z;
	s=1.0;
	double magnitude;
	magnitude=sqrt((x*x+y*y+z*z+s*s));
	if(magnitude==0)
		return;
	x/=magnitude;
	y/=magnitude;
	z/=magnitude;
	s/=magnitude;
}
double dot(vec3d a,vec3d b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}
double Quaterniond::AngleInDirection(const vec3d &vv) const
{
	double dp=dot(vv,vec3d(x,y,z));
	double halfangle=asin(dp);
	ERRNO_BREAK
	return halfangle*2.0;
}
double Quaterniond::Angle() const
{
	double halfangle=acos(std::min(1.0,std::max(-1.0,s)));
	ERRNO_BREAK
	return halfangle*2.0;
}
void Quaterniond::Define(double angle,const vec3d &vv)
{
	angle/=2.0;
	s=cos(angle);
	double ss=sin(angle);
	x=ss*vv.x;
	y=ss*vv.y;
	z=ss*vv.z;
}

void Quaterniond::Define(const vec3d &dir_sin)
{
	double mag=length(dir_sin);
	// sin(a)=2sin(a/2)cos(a/2)
	// so x sin(a) = 2 x sin (a/2) cos (a/2)

	// Thus the quaternion, which is defined as:
	// s=cos(a/2)
	// v=sin(a/2) V
	
	// is given by:
	// a2=asin(mag)/2
	// s=cos(a2)

	// v=sin(a/2) V
	//	=V sin(a)/(2 cos(a/2))
	//	=dir_sin / (2 cos(a/2))

	double a2=asin(mag)/2.0;
	s=cos(a2);
	double div=1.0/(2.0*cos(a2));
	x=dir_sin.x*div;
	y=dir_sin.y*div;
	z=dir_sin.z*div;
}

void Quaterniond::Define(const double ss,const double xx,const double yy,const double zz)
{
	s=ss;
	x=xx;
	y=yy;
	z=zz;
}

Quaterniond Quaterniond::operator*(const Quaterniond &q) const
{
	Quaterniond r;
    r.s= s*q.s - x*q.x - y*q.y - z*q.z;
    r.x= s*q.x + x*q.s + y*q.z - z*q.y;
    r.y= s*q.y + y*q.s + z*q.x - x*q.z;
    r.z= s*q.z + z*q.s + x*q.y - y*q.x;
	return r;
}

Quaterniond Quaterniond::operator/(const Quaterniond &q) const
{
	Quaterniond iq=q;
	iq.x*=-1.0;
	iq.y*=-1.0;
	iq.z*=-1.0;
	return (*this)*iq;
}

Quaterniond& Quaterniond::operator/=(const Quaterniond &q)
{
	double X,Y,Z;
	X=s*q.x+q.s*x+q.y*z-q.z*y;
	Y=s*q.y+q.s*y+q.z*x-q.x*z;
	Z=s*q.z+q.s*z+q.x*y-q.y*x;
	s=q.s*s-q.x*x-q.y*y-q.z*z;

	x=X;
	y=Y;
	z=Z;
	return *this;
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

Quaterniond& Quaterniond::Rotate(double angle,const vec3d &axis)
{
	Quaterniond dq(angle,axis);
	*this=dq*(*this);
	MakeUnit();
	return *this;
}         
void Quaterniond::Rotate(const vec3d &d)
{
	double sz=length(d);
	if(sz>0)
	{
		vec3d a=d;
		a/=sz;
		Quaterniond dq(sz,a);
		double s1=dq.s*s-dq.x*x-dq.y*y-dq.z*z;
		double x1=s*dq.x+dq.s*x+dq.y*z-dq.z*y;
		double y1=s*dq.y+dq.s*y+dq.z*x-dq.x*z;
		double z1=s*dq.z+dq.s*z+dq.x*y-dq.y*x;
		s=s1;
		x=x1;
		y=y1;
		z=z1;
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

	float x_rad = offset.x / sph_radius;
	float y_rad = offset.y / sph_radius;
	crossplatform::Quaterniond x_rot(x_rad, vec3(0.0, 1.0, 0.0));
	crossplatform::Quaterniond y_rot(-y_rad, vec3(1.0, 0.0, 0.0));

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
	
	crossplatform::Quaterniond x_rot(theta, vec3(0.0, 1.0, 0.0));
	crossplatform::Quaterniond z_rot(phi, vec3(0.0, 0.0, 1.0));

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