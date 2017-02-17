#define SIM_MATH
#include "Quaterniond.h"
//---------------------------------------------------------------------------     
#ifdef WIN32
	//#define SIMD
#endif

#ifdef PLAYSTATION_2
	#define PLAYSTATION2
#endif        

#include <math.h>

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
Quaterniond::Quaterniond(double S,double X,double Y,double Z,bool normalize)
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

Quaterniond::Quaterniond(double ss,const vec3d &vv)
{
	Define(ss,vv);
}

Quaterniond::Quaterniond(const double *q)
{
	x=q[0];
	y=q[1];
	z=q[2];
	s=q[3];
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
	x=0.f;
	y=0.f;
	z=0.f;
	s=1.f;
}

void Quaterniond::MakeUnit()
{
	double magnitude;
	magnitude=sqrt((x*x+y*y+z*z+s*s));
	if(magnitude==0)
	{
		s=1.0f;
		return;
	}
	x/=magnitude;
	y/=magnitude;
	z/=magnitude;
	s/=magnitude;
}

void Quaterniond::DefineSmall(double ss,const vec3d &vv)
{
#ifndef PLAYSTATION2
	ss*=0.5f;
	x=ss*vv.x;
	y=ss*vv.y;
	z=ss*vv.z;
	s=1.f;
	double magnitude;
	magnitude=sqrt((x*x+y*y+z*z+s*s));
	if(magnitude==0)
		return;
	x/=magnitude;
	y/=magnitude;
	z/=magnitude;
	s/=magnitude;
#else
	ss;vv;
		asm __volatile__("
			.set noreorder
			add t1,zero,2
			mtc1 t1,$f12
			add t1,zero,1
			mtc1 t1,$f4				// f4 = 1
			lwc1 $f10,0(%1)			// ss   
			//mtc1 %1,$f10		

			add t0,zero,%0			// vv.Values
			cvt.s.w	$f12,$f12
			div.s $f10,$f10,$f12	// f10 = ss/2
			cvt.s.w	$f4,$f4
			
			lwc1		$f1,0(t0)	// load v1[X]
			lwc1		$f2,4(t0)	// load v1[Y]
			lwc1		$f3,8(t0)	// load v1[Z]
			
			mul.s $f1,$f1,$f10
			mul.s $f2,$f2,$f10
			mul.s $f3,$f3,$f10
			
			mula.s	$f1,$f1			// ACC = v1[X] * v2[X]
			madda.s	$f2,$f2			// ACC += v1[Y] * v2[Y]
			madda.s	$f3,$f3			// ACC += v1[Y] * v2[Y]
			madd.s	$f5,$f4,$f4		// rv = ACC + v1[Z] * v2[Z]
			sqrt.s	$f6,$f5
			div.s $f1,$f6
			div.s $f2,$f6
			div.s $f3,$f6
			div.s $f4,$f6
			swc1 $f1, 0(a0)			// store v1[X]
			swc1 $f2, 4(a0)			// store v1[Y]
			swc1 $f3, 8(a0)			// store v1[Z]
			swc1 $f4, 12(a0)			// store v1[Z]
		"					:
							: "r" (vv.Values),"g" (&ss)
							: );
#endif
}
double dot(vec3d a,vec3d b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}
double Quaterniond::AngleInDirection(const vec3d &vv) const
{
	double dp=dot(vv,vec3d(x,y,z));
	double halfangle=asin(dp);
	return halfangle*2.f;
}
double Quaterniond::Angle() const
{
	double halfangle=acos(s);
	return halfangle*2.f;
}
void Quaterniond::Define(double angle,const vec3d &vv)
{
	angle/=2.f;
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

	double a2=asin(mag)/2.f;
	s=cos(a2);
	double div=1.f/(2.f*cos(a2));
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
    r.s= s * q.s - x * q.x - y * q.y - z * q.z;
    r.x= s * q.x + x * q.s + y * q.z - z * q.y;
    r.y= s * q.y + y * q.s + z * q.x - x * q.z;
    r.z= s * q.z + z * q.s + x * q.y - y * q.x;
	return r;
}

Quaterniond Quaterniond::operator/(const Quaterniond &q) const
{
	Quaterniond iq=q;
	iq.x*=-1.f;
	iq.y*=-1.f;
	iq.z*=-1.f;
	return (*this)*iq;
}          
Quaterniond& Quaterniond::operator/=(const Quaterniond &q)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	register double X,Y,Z;
	X=s*q.x+q.s*x+q.y*z-q.z*y;
	Y=s*q.y+q.s*y+q.z*x-q.x*z;
	Z=s*q.z+q.s*z+q.x*y-q.y*x;
	s=q.s*s-q.x*x-q.y*y-q.z*z;

	x=X;
	y=Y;
	z=Z;
#else               
	/*register double X,Y,Z;

	X=s*q.x+q.s*x+q.y*z-q.z*y;
	Y=s*q.y+q.s*y+q.z*x-q.x*z;
	Z=s*q.z+q.s*z+q.x*y-q.y*x;
	s=q.s*s-q.x*x-q.y*y-q.z*z;

	x=X;
	y=Y;
	z=Z;   */
// (X,Y,Z,s) => s*q + q.s*this
//			 +  (z,x,y,0)*q.(y,z,x,0) - (y,z,x,0)*q.(z,x,y,0)
//			s -= this*q
// Alternatively:    
// (X,Y,Z,s) => (s,s,s,s)*q + q.(s,s,s,0)*this
//			 +  (z,x,y,0)*q.(y,z,x,0) - (y,z,x,0)*q.(z,x,y,0)
//			s -= (x,y,z).q(x,y,z)
	//static double temp[4];
	//static int mask[]={0,0,0,0xFFFFFFFF};
	//int *mmm=mask;
	//double *tt=temp;
	_asm
	{
	mov eax,this
	mov ebx,q
//	mov edx,mmm
//mov ecx,tt
	movups xmm1,[eax]
	movups xmm2,[ebx]
	movaps xmm3,xmm1
	movaps xmm4,xmm2
	shufps xmm3,xmm3,255	// xmm3 -> s,s,s,s
//movups [ecx],xmm3
	shufps xmm4,xmm4,255			// xmm4 -> q.s,q.s,q.s,q.s
//movups [ecx],xmm4
	mulps xmm3,xmm2       
//movups [ecx],xmm3
	mulps xmm4,xmm1      
//movups [ecx],xmm4
	addps xmm3,xmm4			// xmm3 = total so far, includes scalar multiplied terms.
//movups [ecx],xmm3
	movaps xmm5,xmm1
	movaps xmm6,xmm2   
	movaps xmm7,xmm1
	mulps xmm7,xmm2	  
//movups [ecx],xmm7
	shufps xmm1,xmm1,18		// 2,0,1
	shufps xmm2,xmm2,9		// 1,2,0
	shufps xmm5,xmm5,9		// 1,2,0
	shufps xmm6,xmm6,18		// 2,0,1
	mulps xmm1,xmm2
	mulps xmm5,xmm6
	addps xmm3,xmm1
	subps xmm3,xmm5			// xmm3 contains all terms except the dot product terms.
//movups [ecx],xmm3
	movaps xmm6,xmm7
	shufps xmm6,xmm6,9
//movups [ecx],xmm6
	addps xmm7,xmm6
//movups [ecx],xmm7
	shufps xmm6,xmm6,1
//movups [ecx],xmm6
	addps xmm7,xmm6
//movups [ecx],xmm7
	movss xmm0,[eax+12]
//movups [ecx],xmm0
	mulss xmm0,[ebx+12]
//movups [ecx],xmm0
	subss xmm0,xmm7
//movups [ecx],xmm0
	movups [eax],xmm3		// store 1st 3
	movss [eax+12],xmm0
	};
#endif
#else
	//double *rVal=&(x);
	const double *Q1=&(x);
	const double *Q2=&(q.x);
	asm __volatile__("
			.set noreorder
		lqc2			vf1,0(%1)			// Load v1's double address into vf1
		lqc2			vf2,0(%2)			// Load v2's double address into vf1
		vmul            vf4,vf1,vf2			// xx yy zz ww
		vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
		vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
		vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
		vopmula.xyz     ACC,vf2,vf1   		// Start Outerproduct
		vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
		vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
		vopmsub.xyz     vf3,vf1,vf2  		// Finish Outerproduct
		sqc2			vf3,0(%0)			// Load vf3 into r's double address
		.set reorder
		"					: /* Outputs. */
							: /* Inputs */ "r" (this),"r" (Q1), "r" (Q2)
							: /* Clobber */ "$vf1");
#endif
	return *this;
}
namespace simul
{
	namespace crossplatform
	{
		void Multiply(Quaterniond &r,const Quaterniond &q1,const Quaterniond &q2)
		{
		#ifndef PLAYSTATION2
			r.s=q1.s*q2.s-q1.x*q2.x-q1.y*q2.y-q1.z*q2.z;
			r.x=q2.s*q1.x+q1.s*q2.x+q1.y*q2.z-q1.z*q2.y;
			r.y=q2.s*q1.y+q1.s*q2.y+q1.z*q2.x-q1.x*q2.z;
			r.z=q2.s*q1.z+q1.s*q2.z+q1.x*q2.y-q1.y*q2.x;
		#else
			//double *rVal=&(r.x);
			const double *Q1=&(q1.x);
			const double *Q2=&(q2.x);
			asm __volatile__("
					.set noreorder
				lqc2			vf1,0(%1)			// Load v1's double address into vf1
				lqc2			vf2,0(%2)			// Load v2's double address into vf1
				vmul            vf4,vf1,vf2			// xx yy zz ww
				vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
				vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
				vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
				vopmula.xyz     ACC,vf1,vf2   		// Start Outerproduct
				vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
				vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
				vopmsub.xyz     vf3,vf2,vf1  		// Finish Outerproduct
				sqc2			vf3,0(%0)			// Load vf3 into r's double address
				"					: /* Outputs. */
									: /* Inputs */  "r" (&r),"r" (Q1), "r" (Q2)
									: /* Clobber */ "$vf1");
		#endif
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
		#ifndef PLAYSTATION2
			r.s=q1.s*q2.s+q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			r.x=q2.s*q1.x-q1.s*q2.x-q1.y*q2.z+q1.z*q2.y;
			r.y=q2.s*q1.y-q1.s*q2.y-q1.z*q2.x+q1.x*q2.z;
			r.z=q2.s*q1.z-q1.s*q2.z-q1.x*q2.y+q1.y*q2.x;
		#else
			//double *rVal=&(r.x);
			const double *Q1=&(q1.x);
			const double *Q2=&(q2.x);
			asm __volatile__("
					.set noreorder
				lqc2			vf1,0(%1)			// Load v1's double address into vf1
				lqc2			vf2,0(%2)			// Load v2's double address into vf1
				vmul            vf4,vf1,vf2			// xx yy zz ww
				vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
				vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
				vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
				vopmula.xyz     ACC,vf1,vf2   		// Start Outerproduct
				vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
				vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
				vopmsub.xyz     vf3,vf2,vf1  		// Finish Outerproduct
				sqc2			vf3,0(%0)			// Load vf3 into r's double address
				"					: /* Outputs. */
									: /* Inputs */  "r" (&r),"r" (Q1), "r" (Q2)
									: /* Clobber */ "$vf1");
		#endif
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
			double dot = q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			if(dot<0)
			{
				dot=-dot;
				Q2.x*=-1.f;
				Q2.y*=-1.f;
				Q2.z*=-1.f;
				Q2.s*=-1.f;
			}
			dot+=q1.s*Q2.s;
			
			const double DOT_THRESHOLD = 0.9995f;
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
			if(dot<-1.f)
				dot=-1.f;
			if(dot>1.f)
				dot=1.f;
			double theta_0 = acos(dot);  // theta_0 = half angle between input vectors
			double theta1 = theta_0*(1.f-l);    // theta = angle between v0 and result 
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

