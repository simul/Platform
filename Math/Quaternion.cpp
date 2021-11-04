#define SIM_MATH
#include "Quaternion.h"
//---------------------------------------------------------------------------     
#ifdef WIN32
	//#define SIMD
#endif

#ifdef PLAYSTATION_2
	#define PLAYSTATION2
#endif        

#include <math.h>

using namespace simul;
using namespace math;

Quaternion::Quaternion()
{
	Reset();
}
Quaternion::Quaternion(float S,float X,float Y,float Z,bool normalize)
{
	x=X;
	y=Y;
	z=Z;
	s=S;
	if(normalize)
		this->MakeUnit();
}

Quaternion::Quaternion(const Quaternion &q)
{
	s=q.s;
	x=q.x;
	y=q.y;
	z=q.z;
}

Quaternion::Quaternion(float ss,const Vector3 &vv)
{
	Define(ss,vv);
}

Quaternion::Quaternion(const float *q)
{
	x=q[0];
	y=q[1];
	z=q[2];
	s=q[3];
}

void Quaternion::Reset()
{
	x=0.f;
	y=0.f;
	z=0.f;
	s=1.f;
}

void Quaternion::MakeUnit()
{
	float magnitude;
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

void Quaternion::DefineSmall(float ss,const Vector3 &vv)
{
#ifndef PLAYSTATION2
	ss*=0.5f;
	x=ss*vv(0);
	y=ss*vv(1);
	z=ss*vv(2);
	s=1.f;
	float magnitude;
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
float Quaternion::AngleInDirection(const Vector3 &vv) const
{
	float dp=vv*Vector3(x,y,z);
	float halfangle=asin(dp);
	return halfangle*2.f;
}
float Quaternion::Angle() const
{
	float halfangle=acos(s);
	return halfangle*2.f;
}
void Quaternion::Define(float angle,const Vector3 &vv)
{
	angle/=2.f;
	s=cos(angle);
	float ss=sin(angle);
	x=ss*vv(0);
	y=ss*vv(1);
	z=ss*vv(2);
}

void Quaternion::Define(const Vector3 &dir_sin)
{
	float mag=dir_sin.Magnitude();
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

	float a2=asin(mag)/2.f;
	s=cos(a2);
	float div=1.f/(2.f*cos(a2));
	x=dir_sin.x*div;
	y=dir_sin.y*div;
	z=dir_sin.z*div;
}

void Quaternion::Define(const float ss,const float xx,const float yy,const float zz)
{
	s=ss;
	x=xx;
	y=yy;
	z=zz;
}

Quaternion Quaternion::operator*(const Quaternion &q) const
{
	Quaternion r;
    r.s= s * q.s - x * q.x - y * q.y - z * q.z;
    r.x= s * q.x + x * q.s + y * q.z - z * q.y;
    r.y= s * q.y + y * q.s + z * q.x - x * q.z;
    r.z= s * q.z + z * q.s + x * q.y - y * q.x;
	return r;
}

Quaternion Quaternion::operator/(const Quaternion &q) const
{
	Quaternion iq=q;
	iq.x*=-1.f;
	iq.y*=-1.f;
	iq.z*=-1.f;
	return (*this)*iq;
}          
Quaternion& Quaternion::operator/=(const Quaternion &q)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	float X,Y,Z;
	X=s*q.x+q.s*x+q.y*z-q.z*y;
	Y=s*q.y+q.s*y+q.z*x-q.x*z;
	Z=s*q.z+q.s*z+q.x*y-q.y*x;
	s=q.s*s-q.x*x-q.y*y-q.z*z;

	x=X;
	y=Y;
	z=Z;
#else               
	/*float X,Y,Z;

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
	//static float temp[4];
	//static int mask[]={0,0,0,0xFFFFFFFF};
	//int *mmm=mask;
	//float *tt=temp;
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
	//float *rVal=&(x);
	const float *Q1=&(x);
	const float *Q2=&(q.x);
	asm __volatile__("
			.set noreorder
		lqc2			vf1,0(%1)			// Load v1's float address into vf1
		lqc2			vf2,0(%2)			// Load v2's float address into vf1
		vmul            vf4,vf1,vf2			// xx yy zz ww
		vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
		vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
		vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
		vopmula.xyz     ACC,vf2,vf1   		// Start Outerproduct
		vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
		vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
		vopmsub.xyz     vf3,vf1,vf2  		// Finish Outerproduct
		sqc2			vf3,0(%0)			// Load vf3 into r's float address
		.set reorder
		"					: /* Outputs. */
							: /* Inputs */ "r" (this),"r" (Q1), "r" (Q2)
							: /* Clobber */ "$vf1");
#endif
	return *this;
}
namespace simul
{
	namespace math
	{
		void Multiply(Quaternion &r,const Quaternion &q1,const Quaternion &q2)
		{
		#ifndef PLAYSTATION2
			r.s=q1.s*q2.s-q1.x*q2.x-q1.y*q2.y-q1.z*q2.z;
			r.x=q2.s*q1.x+q1.s*q2.x+q1.y*q2.z-q1.z*q2.y;
			r.y=q2.s*q1.y+q1.s*q2.y+q1.z*q2.x-q1.x*q2.z;
			r.z=q2.s*q1.z+q1.s*q2.z+q1.x*q2.y-q1.y*q2.x;
		#else
			//float *rVal=&(r.x);
			const float *Q1=&(q1.x);
			const float *Q2=&(q2.x);
			asm __volatile__("
					.set noreorder
				lqc2			vf1,0(%1)			// Load v1's float address into vf1
				lqc2			vf2,0(%2)			// Load v2's float address into vf1
				vmul            vf4,vf1,vf2			// xx yy zz ww
				vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
				vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
				vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
				vopmula.xyz     ACC,vf1,vf2   		// Start Outerproduct
				vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
				vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
				vopmsub.xyz     vf3,vf2,vf1  		// Finish Outerproduct
				sqc2			vf3,0(%0)			// Load vf3 into r's float address
				"					: /* Outputs. */
									: /* Inputs */  "r" (&r),"r" (Q1), "r" (Q2)
									: /* Clobber */ "$vf1");
		#endif
		}           
		//------------------------------------------------------------------------------
		void MultiplyNegativeByQuaternion(Quaternion &r,const Quaternion &q1,const Quaternion &q2)
		{
			r.s=q1.s*q2.s +q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			r.x=-q2.s*q1.x+q1.s*q2.x-q1.y*q2.z+q1.z*q2.y;
			r.y=-q2.s*q1.y+q1.s*q2.y-q1.z*q2.x+q1.x*q2.z;
			r.z=-q2.s*q1.z+q1.s*q2.z-q1.x*q2.y+q1.y*q2.x;
		}
		void MultiplyByNegative(Quaternion &r,const Quaternion &q1,const Quaternion &q2)
		{
		#ifndef PLAYSTATION2
			r.s=q1.s*q2.s+q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			r.x=q2.s*q1.x-q1.s*q2.x-q1.y*q2.z+q1.z*q2.y;
			r.y=q2.s*q1.y-q1.s*q2.y-q1.z*q2.x+q1.x*q2.z;
			r.z=q2.s*q1.z-q1.s*q2.z-q1.x*q2.y+q1.y*q2.x;
		#else
			//float *rVal=&(r.x);
			const float *Q1=&(q1.x);
			const float *Q2=&(q2.x);
			asm __volatile__("
					.set noreorder
				lqc2			vf1,0(%1)			// Load v1's float address into vf1
				lqc2			vf2,0(%2)			// Load v2's float address into vf1
				vmul            vf4,vf1,vf2			// xx yy zz ww
				vsubaz          ACC,vf4,vf4z		// ACC.w is now vf4.w - vf4.z, which is ww-zz
				vmsubay         ACC,vf0,vf4y		// ACC.w is now ACC.w*vf0.w - vf4.y, which is ww - zz - yy
				vmsubx	       	vf3,vf0,vf4x		// vf3.w is now ACC.w*vf0.w - vf4.x, which is ww - zz - yy - xx
				vopmula.xyz     ACC,vf1,vf2   		// Start Outerproduct
				vmaddaw       	ACC,vf1,vf2w		// Add w2.xyz1
				vmaddaw			ACC,vf2,vf1w		// Add w1.xyz2
				vopmsub.xyz     vf3,vf2,vf1  		// Finish Outerproduct
				sqc2			vf3,0(%0)			// Load vf3 into r's float address
				"					: /* Outputs. */
									: /* Inputs */  "r" (&r),"r" (Q1), "r" (Q2)
									: /* Clobber */ "$vf1");
		#endif
		}
		// v must be a 3-vector, ret must be a 3-vector.
		void Multiply(Vector3 &ret,const Quaternion &q,const Vector3 &v)
		{
			const float &x0=v.Values[0];
			const float &y0=v.Values[1];
			const float &z0=v.Values[2];
			float s1=q.x*x0+q.y*y0+q.z*z0;
			float x1=q.s*x0+q.y*z0-q.z*y0;
			float y1=q.s*y0+q.z*x0-q.x*z0;
			float z1=q.s*z0+q.x*y0-q.y*x0;
			ret.Values[0]=s1*q.x+q.s*x1+q.y*z1-q.z*y1;
			ret.Values[1]=s1*q.y+q.s*y1+q.z*x1-q.x*z1;
			ret.Values[2]=s1*q.z+q.s*z1+q.x*y1-q.y*x1;
		}             
		// v must be a 3-vector, ret must be a 3-vector.
		void AddQuaternionTimesVector(Vector3 &ret,const Quaternion &q,const Vector3 &v)
		{
			const float &x0=v.Values[0];
			const float &y0=v.Values[1];
			const float &z0=v.Values[2];
			float s1=q.x*x0+q.y*y0+q.z*z0;
			float x1=q.s*x0+q.y*z0-q.z*y0;
			float y1=q.s*y0+q.z*x0-q.x*z0;
			float z1=q.s*z0+q.x*y0-q.y*x0;
			ret.Values[0]+=s1*q.x+q.s*x1+q.y*z1-q.z*y1;
			ret.Values[1]+=s1*q.y+q.s*y1+q.z*x1-q.x*z1;
			ret.Values[2]+=s1*q.z+q.s*z1+q.x*y1-q.y*x1;
		}
		void Divide(Vector3 &ret,const Quaternion &q,const Vector3 &v)
		{
			const float &x0=v.Values[0];
			const float &y0=v.Values[1];
			const float &z0=v.Values[2];
			 float s1;
			s1=-q.x*x0-q.y*y0-q.z*z0;
			 float x1;
			x1= q.s*x0-q.y*z0+q.z*y0;
			 float y1;
			y1= q.s*y0-q.z*x0+q.x*z0;
			 float z1;
			z1= q.s*z0-q.x*y0+q.y*x0;
			ret.Values[0]=-s1*q.x+q.s*x1-q.y*z1+q.z*y1,
			ret.Values[1]=-s1*q.y+q.s*y1-q.z*x1+q.x*z1,
			ret.Values[2]=-s1*q.z+q.s*z1-q.x*y1+q.y*x1;
		}
		//------------------------------------------------------------------------------
		void FPUQuaternionToMatrix(Matrix4x4 &M,const Quaternion &q)
		{
			const float X=q.x;
			const float Y=q.y;
			const float Z=q.z;
			const float S=q.s;
			float SS = q.s*q.s;
			float XX = q.x*q.x;
			float YY = q.y*q.y;
			float ZZ = q.z*q.z;
			//float invs = 1.0f/ (sqx + sqy + sqz + sqs);
			M.Values[    0]=(float)(XX-YY-ZZ+SS);
			M.Values[    1]=(float)(2.0*(X*Y-Z*S));      // -!!    
			M.Values[    2]=(float)(2.0*(X*Z+Y*S));      // +!!

			M.Values[  4  ]=(float)(2.0*(X*Y+Z*S));      // +
			M.Values[  4+1]=(float)(-X*X+Y*Y-Z*Z+S*S);       
			M.Values[  4+2]=(float)(2.0*(Y*Z-X*S));      // was -!!

			M.Values[2*4  ]=(float)(2.0*(X*Z-Y*S));      // -
			M.Values[2*4+1]=(float)(2.0*(Y*Z+X*S));      // was + !!
			M.Values[2*4+2]=(float)(-X*X-Y*Y+Z*Z+S*S);
		} 
		//------------------------------------------------------------------------------
		void QuaternionToMatrix(Matrix4x4 &M,const Quaternion &q)
		{
			double X=(double)q.x;
			double Y=(double)q.y;
			double Z=(double)q.z;
			double S=(double)q.s;
			double sqw = S*S;
			double sqx = X*X;
			double sqy = Y*Y;
			double sqz = Z*Z;
			double invs = 1.0/ (sqx + sqy + sqz + sqw);
			M.Values[    0]=(float)((sqx-sqy-sqz+sqw)*invs);//    = (1-2*(Y*Y+Z*Z));= (X*X+Y*Y+Z*Z+S*S-2*(Y*Y+Z*Z))  // X*X+Y*Y+Z*Z+S*S=1.      
			M.Values[    1]=(float)(2.0*(X*Y+Z*S));      // +     
			M.Values[    2]=(float)(2.0*(X*Z-Y*S));      // -

			M.Values[  4+0]=(float)(2.0*(X*Y-Z*S));      // -!!  
			M.Values[  4+1]=(float)(-X*X+Y*Y-Z*Z+S*S);        
			M.Values[  4+2]=(float)(2.0*(Y*Z+X*S));      // was + !!

			M.Values[2*4+0]=(float)(2.0*(X*Z+Y*S));      // +!!
			M.Values[2*4+1]=(float)(2.0*(Y*Z-X*S));      // was -!!
			M.Values[2*4+2]=(float)(-X*X-Y*Y+Z*Z+S*S);
		}
		void MatrixToQuaternion(Quaternion &q,const Matrix4x4 &M)
		{
			Matrix4x4 T;
			M.Transpose(T);
		//    Calculate the trace of the matrix T from the equation:
			  double Tr=(double)T(0,0)+(double)T(1,1)+(double)T(2,2)+1.0;
		//    If the trace of the matrix is greater than zero
			if(Tr>0)
			{           
				double Size=2.0*sqrt(Tr);
				q.s=(float)(0.25*Size);
				q.x=(float)((T.m21-T.m12)/Size);
				q.y=(float)((T.m02-T.m20)/Size);
				q.z=(float)((T.m10-T.m01)/Size);
			}
			else
			{
				float  r[4];
				int    i, j, k;
				int nxt[3] = {1, 2, 0};
			 // diagonal is negative
				i = 0;
				if (T(1,1) > T(0,0))
					i = 1;
				if (T(2,2) > T(i,i))
					i = 2;
				j = nxt[i];
				k = nxt[j];
				float Size = sqrt ((T(i,i) - (T(j,j) + T(k,k))) + 1.0f);
				r[i] = Size * 0.5f;
				if (Size != 0.0f)
					Size = 0.5f / Size;
				r[j] = (T(i,j) + T(j,i)) * Size;
				r[k] = (T(i,k) + T(k,i)) * Size;
				r[3] = (T(j,k) - T(k,j)) * Size;

				q.x = -r[0];
				q.y = -r[1];
				q.z = -r[2];
				q.s = r[3];
			}
			q.MakeUnit();
		}

		void Slerp(Quaternion &ret,const Quaternion &q1,const Quaternion &q2,float l)
		{
			// v0 and v1 should be unit length or else
			// something broken will happen.

			// Compute the cosine of the angle between the two vectors.
			Quaternion Q2=q2;
			float dot = q1.x*q2.x+q1.y*q2.y+q1.z*q2.z;
			if(dot<0)
			{
				dot=-dot;
				Q2.x*=-1.f;
				Q2.y*=-1.f;
				Q2.z*=-1.f;
				Q2.s*=-1.f;
			}
			dot+=q1.s*Q2.s;
			
			const float DOT_THRESHOLD = 0.9995f;
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
			float theta_0 = acos(dot);  // theta_0 = half angle between input vectors
			float theta1 = theta_0*(1.f-l);    // theta = angle between v0 and result 
			float theta2 = theta_0*l;    // theta = angle between v0 and result 

			float s1=sin(theta1);
			float s2=sin(theta2);
			float ss=sin(theta_0);
			/*Quaternion q3;
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
