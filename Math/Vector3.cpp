#define SIM_MATH
#include "Platform/Math/VirtualVector.h"
#include "Platform/Math/Vector3.h"
#include "Platform/Math/SimVector.h"
#include <math.h>
#include <algorithm>
#include <iostream>

#include "Vector3.inl"

#undef PLAYSTATION2
#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#else
#undef CHECK_MATRIX_BOUNDS
#endif
#ifndef SIMD

namespace simul
{
	namespace math
	{
float operator*(const Vector3 &v1,const Vector3 &v2)
{
#ifndef PLAYSTATION2
	float total=0.0f;
	for(unsigned i=0;i<3;i++)
		total+=v1.Values[i]*v2.Values[i];
	return total;
#else
	float d;
	asm __volatile__("  
		.set noreorder
		lwc1		$f1, 0(%1)			// load v1[X]
		lwc1		$f2, 0(%2)			// load v2[X]
		lwc1		$f3, 4(%1)			// load v1[Y]
		lwc1		$f4, 4(%2)			// load v2[Y]
		lwc1		$f5, 8(%1)			// load v1[Z]
		lwc1		$f6, 8(%2)			// load v2[Z]
		mula.s	$f1, $f2				// ACC = v1[X] * v2[X]
		madda.s	$f3, $f4				// ACC += v1[Y] * v2[Y]
		madd.s	%0, $f5, $f6		// rv = ACC + v1[Z] * v2[Z]
	"					: "=f" (d)
						:"r" (v1.Values), "r" (v2.Values)
						: );
	return d;
#endif
}
#endif
struct BadAlignment{};
Vector3::Vector3()
{
/*#ifdef SIMD
	_asm
	{
		xorps xmm0,xmm0
		mov ebx,this
		movaps [ebx],xmm0
	}
#else*/
	Values[3]=1e15f;
	//Values[1]=20e15f;
	//Values[2]=300e15f;
	//Values[3]=4000e15f;
//#endif
}
//------------------------------------------------------------------------------
Vector3::Vector3(float a,float b,float c)
{
	Values[0]=a;
	Values[1]=b;
	Values[2]=c;
	Values[3]=0;
}
Vector3::Vector3(const float *x)
{
	Values[0]=x[0];
	Values[1]=x[1];
	Values[2]=x[2];
	Values[3]=0;
}       
//------------------------------------------------------------------------------
Vector3::Vector3(const Vector3 &v)
{
	operator=(v);
}

Vector3::operator const float*() const
{
	return Values;
}
Vector3::operator float*()
{
	return Values;
}
void Vector3::Define(float x,float y,float z)
{
	Values[0]=x;
	Values[1]=y;
	Values[2]=z;
}
void Vector3::DefineValues(const float *x)
{
	Values[0]=x[0];
	Values[1]=x[1];
	Values[2]=x[2];
}       
//-----------------------------------------------------------------------------
void Vector3::Zero()
{
#ifdef SIMD
	_asm
	{
		xorps xmm0,xmm0
		mov ebx,this
		movaps [ebx],xmm0
	}
#else
#ifdef PLAYSTATION2
	asm __volatile__
	(
	"
		.set noreorder
		add t0,zero,%0
		add t2,zero,3
		blez t2,iEscape
		vsub vf1,vf0,vf0
		iLoop:
			sqc2 vf1,0(t0)
			addi t2,-4
		bgtz t2,iLoop
			addi t0,16
		iEscape:
		.set reorder
	"	:
		: "r" (Values)
		:  );
#else
	unsigned i;
	for(i=0;i<4;++i)
		Values[i]=0.f;
#endif
#endif
}
//------------------------------------------------------------------------------
bool Vector3::NonZero() const
{
	for(unsigned i=0;i<3;i++)
	{
		if(Values[i]*Values[i]>1e-16f)
			return true;
	}
	return false;
}
float &Vector3::operator() (unsigned index)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(index>=3)
		throw BadSize();
#endif
	return Values[index];
}
/// Return the value at index index, which should be 0, 1 or 2.
float Vector3::operator() (unsigned index) const
{                
#ifdef CHECK_MATRIX_BOUNDS
	if(index>=3)
		throw BadSize();
#endif
	return Values[index];
}
//------------------------------------------------------------------------------
void Vector3::operator=(const Vector3 &v)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<4;i++)
		Values[i]=v.Values[i];
#else
	asm __volatile__("     
		.set noreorder
		add t2,zero,%1
		lqc2 vf2,0(t2)		// Load 4 numbers into vf1
		add t1,zero,%0
		sqc2 vf2,0(t1)		// Load 4 numbers into vf2
		.set reorder
	"					:
						: "r" (Values), "r" (v.Values)
						:  "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	__asm
	{
		mov edi,this
		mov esi,v
		mov ecx,4

		rep movsd
	}
#endif
}
//------------------------------------------------------------------------------
void Vector3::operator*=(float f)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]*=f;
#else			// line_d*a
	f;
	asm __volatile__("
		.set noreorder  
		lwc1 f2,0(%1)
		mfc1 t8,f2			// t8=f2
		ctc2 t8,$vi21		// put in the immediate I
		nop
		nop
		nop
		add t1,zero,%0		// t1= address of matrix 1's row
		lqc2 vf1,0(t1)		// Load 4 numbers into vf1
		vmuli vf2,vf1,I		
		sqc2 vf2,0(t1)		// next four numbers
		Escape:
	"					: 
						: "r" (Values),"r"(&f)
						: );
#endif
#else
	Values[0]*=f;
	Values[1]*=f;
	Values[2]*=f;
#endif
}
void Vector3::operator/=(float f)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]/=f;
#else
	asm __volatile__("
		.set noreorder
//		CHECK THIS
		add t0,zero,3				// V1.size
		blez t0,Escape				// for zero width matrix
		add t2,zero,%1
		and t3,t2,15
		and t2,t2,0xFFFFFFF0
		lqc2 vf4,0(t2)
		beq t3,0,CaseZero
		beq t3,4,Case1
		beq t3,8,Case2
		nop
		Case3:
		b AllCases
		vdiv Q,vf0w,vf4w
		Case2:
		b AllCases
		vdiv Q,vf0w,vf4z
		Case1:
		b AllCases
		vdiv Q,vf0w,vf4y
		CaseZero:
		vdiv Q,vf0w,vf4x
		AllCases:
		add t1,zero,%0				// t1= address of matrix 1's row
		vwaitq
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			vmulq vf2,vf1,Q
			sqc2 vf2,0(t1)
			addi t0,-4				// counter-=4
		bgtz t0,iLoop
			addi t1,16				// next four numbers
		Escape:
	"					: 
						: "r"(Values),"r"(&f)
						: );
#endif
#else
	__asm
	{
		mov ebx,this			// Values
		movaps xmm1,[ebx]
		movss xmm0,f
		shufps xmm0,xmm0,0x0
		divps xmm1,xmm0
		movaps [ebx],xmm1
	}
#endif
}
void Vector3::operator/=(const Vector3 &v)
{
	x/=v.x;
	y/=v.y;
	z/=v.z;
}
Vector3 operator/(const Vector3 &v1,const Vector3 &v2)
{
	Vector3 v=v1;
	v.x/=v2.x;
	v.y/=v2.y;
	v.z/=v2.z;
	return v;
}
//------------------------------------------------------------------------------
void Vector3::operator-=(const Vector3 &v)
{
#ifndef SIMD        
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<4;++i)
		Values[i]-=v.Values[i];
#else
	asm __volatile__("  
		.set noreorder
			lwc1 f1,0(%0)
			lwc1 f2,0(%1)
			sub.s f3,f1,f2
			swc1 f3,0(%0)
			lwc1 f1,4(%0)
			lwc1 f2,4(%1)
			sub.s f3,f1,f2
			swc1 f3,4(%0)
			lwc1 f1,8(%0)
			lwc1 f2,8(%1)
			sub.s f3,f1,f2
			swc1 f3,8(%0)
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "g" (Values), "g" (v.Values)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	__asm
	{
		mov eax,this
		movaps xmm0,[eax]
		mov ebx,v
		subps xmm0,[ebx]
		movaps [eax],xmm0
	}
#endif
} 
//------------------------------------------------------------------------------
void Vector3::operator+=(const Vector3 &v)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]+=v.Values[i];
#else
	asm __volatile__("  
		.set noreorder
		blez %2,Escape
		Loop:
			lqc2 vf2,0(%1)
			addi %1,16
			lqc2 vf1,0(%0)
			vadd vf3,vf1,vf2
			sqc2 vf3,0(%0)
			addi %2,-4
		bgtz %2,Loop
			addi %0,16
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "g" (Values), "g" (v.Values),"g"(v.size)
						: );
#endif
#else
	__asm
	{
		mov ebx,v   // ebx=v.Values    
		mov eax,this
		movaps xmm0,[ebx]
		addps xmm0,[eax]
		movaps [eax],xmm0
	}
#endif
}
void Vector3::operator*=(const Vector3 &v)
{
	x*=v.x;
	y*=v.y;
	z*=v.z;
}

void Vector3::operator+=(float f)
{
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]+=f;
}
void Vector3::operator-=(float f)
{
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]-=f;
}
//------------------------------------------------------------------------------
Vector3 Vector3::operator-() const
{
	ALIGN16 Vector3 temp;
	for(unsigned i=0;i<3;++i)
		temp.Values[i]=-Values[i];
	return temp;
}
//------------------------------------------------------------------------------
Vector3 operator^(const Vector3 &v1,const Vector3 &v2)
{
	ALIGN16 Vector3 v;
	v.Values[0]=v1.Values[1]*v2.Values[2]-v2.Values[1]*v1.Values[2];
	v.Values[1]=v1.Values[2]*v2.Values[0]-v2.Values[2]*v1.Values[0];
	v.Values[2]=v1.Values[0]*v2.Values[1]-v2.Values[0]*v1.Values[1];
	return v;
}
//------------------------------------------------------------------------------
bool Vector3::Normalize()
{
	return Unit();
}
bool Vector3::Unit()
{           
#if 1//ndef SIMD
#ifndef PLAYSTATION2
	float sz=Magnitude();
	if(!sz)
		return false;
	for(unsigned i=0;i<3;i++)
		Values[i]/=sz;
	return true;
#else
	float *VV=Values;
		asm __volatile__("
			.set noreorder
			add t0,zero,%0
			lwc1		$f1, 0(t0)	// load v1[X]
			lwc1		$f2, 4(t0)	// load v1[Y]
			lwc1		$f3, 8(t0)	// load v1[Z]
			mula.s	$f1, $f1		// ACC = v1[X] * v2[X]
			madda.s	$f2, $f2		// ACC += v1[Y] * v2[Y]
			madd.s	$f4, $f3, $f3	// rv = ACC + v1[Z] * v2[Z]
			sqrt.s	$f5,$f4
			div.s $f1,$f5
			div.s $f2,$f5
			div.s $f3,$f5
			swc1 $f1, 0(t0)			// store v1[X]
			swc1 $f2, 4(t0)			// store v1[Y]
			swc1 $f3, 8(t0)			// store v1[Z]
		"					: /* Outputs. */
							: /* Inputs */ "r" (VV)
							: /* Clobber */ "$f1", "$f3", "$f5");
		return true;
						
#endif
#else
	/* FPU:
	float sz=0.0f;
	for(unsigned i=0;i<3;i++)
		sz+=Values[i]*Values[i];
	_asm
	{
		fld sz
		fsqrt
		fstp sz
	}
	if(!sz)
		return false;
	for(unsigned i=0;i<3;i++)
		Values[i]/=sz;
	return true;*/
	_asm
	{
		mov eax,this
		movaps xmm5,[eax]	
		movaps xmm0,xmm5
		mulps xmm0,xmm0		//xmm0.x=x2
		movaps xmm1,xmm0
		shufps xmm1,xmm1,1	//xmm1.x=y2
		pshufd xmm2,xmm0,2	//xmm2.x=z2

		addss xmm0,xmm1
		addss xmm0,xmm2
		rsqrtss xmm2,xmm0
		shufps xmm2,xmm2,0	// all values = 1/r2
		mulps xmm5,xmm2
		movaps [eax],xmm5
	}
#endif
}
//------------------------------------------------------------------------------
// Size
//------------------------------------------------------------------------------ 
bool Vector3::operator>(float R) const
{
	float *V=(float *)Values;
	float x=*V;
	if(x>R)
		return true;  
	if(-x>R)
		return true;   
	float y=*(++V);
	if(y>R)
		return true;   
	if(-y>R)
		return true;   
	float z=*(++V);
	if(z>R)
		return true;
	if(-z>R)
		return true;  
#ifndef PLAYSTATION2
#ifndef SIMD
	float total=0.0f;
	for(unsigned i=0;i<3;i++)
		total+=Values[i]*Values[i];
	total=sqrt(total);
	return (total>R);
#else
		static float total;
		static float *t3=&total;
		__asm
		{
			mov eax,this
			movss xmm0,[eax]
			mov ecx,t3
			movss xmm1,[eax+4]
			mulss xmm0,xmm0
			movss xmm2,[eax+8]
			mulss xmm1,xmm1
			mulss xmm2,xmm2
			addss xmm0,xmm1
			addss xmm0,xmm2
			sqrtss xmm0,xmm0
			movss [ecx],xmm0
		}
	return (total>R);
#endif
#else
	float total;
	asm __volatile__("
		.set noreorder
		lwc1		f1, 0(%1)			// load v1[X]
		lwc1		f3, 4(%1)			// load v1[Y]
		lwc1		f5, 8(%1)			// load v1[Z]
		mula.s	f1, f1				// ACC = v1[X] * v2[X]
		madda.s	f3, f3				// ACC += v1[Y] * v2[Y]
		madd.s	%0, f5, f5		// rv = ACC + v1[Z] * v2[Z]
	"					: /* Outputs. */ "=f" (total)
						: /* Inputs */ "r" (Values)
						: /* Clobber */ "$f1", "$f3", "$f5");
	return (total>R*R);
#endif      
}
//------------------------------------------------------------------------------
float Vector3::Magnitude() const
{           
	unsigned i;
	float total=0.0f;
	for(i=0;i<3;i++)
		total+=Values[i]*Values[i];
	return sqrt(total);
}
//------------------------------------------------------------------------------
float Vector3::SquareSize() const
{
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i;
	float total=0.0f;
	for(i=0;i<3;i++)
		total+=Values[i]*Values[i];
#ifdef SIMUL_EXCEPTIONS
	if(total<0||total>1e24)
		throw BadNumber();
#endif
	return total;
#else         
	float total=0.0f;
	for(unsigned i=0;i<3;i++)
		total+=Values[i]*Values[i];
	return total;
#endif
#else
	if(size<=3)
	{
		float d;
		asm __volatile__("
			.set noreorder
			lwc1		$f1, 0(%1)			// load v1[X]
			lwc1		$f3, 4(%1)			// load v1[Y]
			lwc1		$f5, 8(%1)			// load v1[Z]
			mula.s	$f1, $f1				// ACC = v1[X] * v2[X]
			madda.s	$f3, $f3				// ACC += v1[Y] * v2[Y]
			madd.s	%0, $f5, $f5		// rv = ACC + v1[Z] * v2[Z]
		"					: /* Outputs. */ "=f" (d)
							: /* Inputs */ "r" (Values)
							: /* Clobber */ "$f1", "$f3", "$f5");
		return d;
	}
	else
	{
		unsigned i;
		float total=0.0f;
		for(i=0;i<size;i++)
		{
			total+=Values[i]*Values[i];
		}
		return total;
	}
#endif
}
//------------------------------------------------------------------------------
void Vector3::AddLarger(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(3>v.size)
		throw BadSize();
#endif
#ifndef SIMD
	for(unsigned i=0;i<3;i++)
		Values[i]+=v(i);
#else
	__asm
	{
		mov eax,this
		mov ecx,v
		mov ecx,[ecx+4]
		movss xmm3,[ecx]
		movss xmm4,[ecx+4]
		movss xmm5,[ecx+8]
		movss xmm0,[eax]
		movss xmm1,[eax+4]
		movss xmm2,[eax+8]
		addss xmm0,xmm3
		addss xmm1,xmm4
		addss xmm2,xmm5
		movss [eax],xmm0
		movss [eax+4],xmm1
		movss [eax+8],xmm2
	}
#endif
}
//------------------------------------------------------------------------------
bool Vector3::ContainsNAN() const
{
	for(unsigned i=0;i<3;i++)
	{
#ifdef WIN32
		if(_isnan(Values[i]))
#else
	#if defined(SN_TARGET_PS3) || defined(__GNUC__)
		if(isnan(Values[i]))
	#else
		if(_isnan(Values[i]))
	#endif
#endif
		return true;
	}
	return false;
}
// Calculation operators
Vector3 operator+(const Vector3 &v1,const Vector3 &v2)
{
	ALIGN16 Vector3 v;
	for(unsigned i=0;i<3;i++)
		v(i)=v1.Values[i]+v2.Values[i];
	return v;
}
Vector3 operator-(const Vector3 &v1,const Vector3 &v2)
{
	ALIGN16 Vector3 v;
	for(unsigned i=0;i<3;i++)
		v.Values[i]=v1.Values[i]-v2.Values[i];
	return v;
}       
float operator*(const Vector3 &v1,const Vector &v2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	float total=0.0f;
	if(3>v2.size)
		return 0;
	for(unsigned i=0;i<3;i++)
		total+=v1.Values[i]*v2.Values[i];
	return total;
#else
	float d;
	asm __volatile__("  
		.set noreorder
		lwc1		$f1, 0(%1)			// load v1[X]
		lwc1		$f2, 0(%2)			// load v2[X]
		lwc1		$f3, 4(%1)			// load v1[Y]
		lwc1		$f4, 4(%2)			// load v2[Y]
		lwc1		$f5, 8(%1)			// load v1[Z]
		lwc1		$f6, 8(%2)			// load v2[Z]
		mula.s	$f1, $f2				// ACC = v1[X] * v2[X]
		madda.s	$f3, $f4				// ACC += v1[Y] * v2[Y]
		madd.s	%0, $f5, $f6		// rv = ACC + v1[Z] * v2[Z]
	"					: "=f" (d)
						:"r" (v1.Values), "r" (v2.Values)
						: );
	return d;
	
#endif
#else
	float temp;
	__asm
	{
		mov eax,v1
		movss xmm0,[eax]
		movss xmm1,[eax+4]
		movss xmm2,[eax+8]
		mov ecx,v2 
		mov ecx,[ecx+4]
		mulss xmm0,[ecx]
		mulss xmm1,[ecx+4]
		mulss xmm2,[ecx+8]
		addss xmm0,xmm1
		addss xmm0,xmm2
		movss temp,xmm0
	}
	return temp;
#endif
}
//------------------------------------------------------------------------------
Vector3 operator*(float d,const Vector3 &v2)
{
	ALIGN16 Vector3 v;
	for(unsigned i=0;i<3;i++)
		v(i)=d*v2.Values[i];
	return v;
}
Vector3 operator*(const Vector3 &v1,float d)
{
	unsigned i;
	ALIGN16 Vector3 v;
	for(i=0;i<3;i++)
		v(i)=d*v1.Values[i];
	return v;
}
Vector3 operator / (const Vector3 &v1,float d)
{
	unsigned i;
	ALIGN16 Vector3 v;
	for(i=0;i<3;i++)
		v.Values[i]=v1.Values[i]/d;
	return v;
}
//------------------------------------------------------------------------------
bool operator!=(Vector3 &v1,Vector3 &v2)
{
	bool result;
	unsigned i;
	result=false;
	for(i=0;i<3;i++)
	{
		if(v1(i)!=v2(i))
			result=true;
	}
	return result;
}            
//------------------------------------------------------------------------------
void Add(Vector3 &R,const VirtualVector &X1,const VirtualVector &X2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(X1.size!=3||X2.size!=3)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector3::BadSize();
	}           
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]+X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vadd vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov esi,X1
		mov ebx,[esi+4]    
		movaps xmm0,[ebx]
		mov ecx,X2
		mov ecx,[esi+4]
		addps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void Subtract1(Vector3 &R,const Vector3 &X1,const Vector3 &X2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]-X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vsub vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov ebx,X1  
		movaps xmm0,[ebx]
		mov ecx,X2
		subps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void Add1(Vector3 &R,const Vector3 &X1,const Vector3 &X2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]+X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vadd vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov ebx,X1 
		movaps xmm0,[ebx]
		mov ecx,X2
		addps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void Subtract(Vector3 &R,const Vector3 &X1,const VirtualVector &X2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]-X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vsub vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov ebx,X1  
		movaps xmm0,[ebx]
		mov esi,X2
		mov ecx,[esi+4]
		subps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void SwapVector3(Vector3 &V1,Vector3 &V2)
{
	std::swap(V1.Values[0],V2.Values[0]);
	std::swap(V1.Values[1],V2.Values[1]);
	std::swap(V1.Values[2],V2.Values[2]);
}              
//------------------------------------------------------------------------------
void AddFloatTimesVector(Vector3 &V2,float f,const Vector3 &V1)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<3;i++)
		V2.Values[i]+=V1.Values[i]*f;
#else
	__asm
	{
		movss xmm1,f
		mov ebx,V1
		shufps xmm1,xmm1,0
		movaps xmm0,[ebx]
		mov eax,V2
		mulps xmm0,xmm1
		addps xmm0,[eax]
		movaps [eax],xmm0
	}
#endif
#else
#ifdef CHECK_MATRIX_BOUNDS
	if(V2.size!=V1.size)
		throw BadSize();
	if(V2.size==0)
		throw BadSize();
#endif
	static float d[4];
	d[0]=f;
	asm __volatile__("
		.set noreorder  
		lqc2 vf4,0(%3)
		vsub vf3,vf00,vf00
		vaddx vf3,vf3,vf4x			// All 4 numbers are equal to multiplier
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape				// for zero width matrix 
		and t3,%1,15				// And with 0xF to see if it's aligned - if pos is a multiple of 4
		nop
		bnez t3,uLoop				// if not, do the unaligned copy
		and t3,%2,15				// And with 0xF to see if it's aligned - if pos is a multiple of 4
		nop
		bnez t3,uLoop				// if not, do the unaligned copy
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			lqc2 vf2,0(t2)			// Load 4 numbers into vf2
			vmul vf4,vf1,vf3
			vadd vf2,vf2,vf4
			sqc2 vf2,0(t2)
			addi t1,16				// next four numbers
			addi t0,-4				// counter-=4
		bgtz t0,iLoop
			addi t2,16
		b Escape
		nop
		uLoop:
			lwc1 f1,0(t1)			// Load 4 numbers into vf1
			lwc1 f2,0(t2)			// Load 4 numbers into vf2
			mul.s f4,f1,f3
			add.s f2,f2,f4
			swc1 f2,0(t2)
			addi t1,4				// next four numbers
			addi t0,-1				// counter-=4
		bgtz t0,uLoop
			addi t2,4
		Escape:
	"					: 
						: "r"(V2.size), "r" (V1.Values), "r" (V2.Values),"r" (d)
						: "t1", "t2");
#endif
}          
//------------------------------------------------------------------------------
void AddFloatTimesVector(Vector3 &V3,const Vector3 &V2,const float f,const Vector3 &V1)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<3;i++)
		V3.Values[i]=V2.Values[i]+V1.Values[i]*f;
#else
	__asm
	{
		mov ebx,V1
		movaps xmm0,[ebx]
		movss xmm1,f
		shufps xmm1,xmm1,0
		mov eax,V2
		movaps xmm2,[eax]
		mulps xmm0,xmm1
		mov ecx,V3
		addps xmm0,xmm2
		movaps [ecx],xmm0
	}
#endif
#else
#ifdef CHECK_MATRIX_BOUNDS
	if(V2.size!=V1.size)
		throw BadSize();
	if(V2.size==0)
		throw BadSize();
#endif
	static float d[4];
	d[0]=f;
	asm __volatile__("
		.set noreorder  
		lqc2 vf4,0(%0)
		vsub vf3,vf00,vf00
		vaddx vf3,vf3,vf4x			// All 4 numbers are equal to multiplier
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2
		add t3,zero,%3
		lqc2 vf1,0(t1)			// Load 4 numbers into vf1
		lqc2 vf2,0(t2)			// Load 4 numbers into vf2
		vmul vf4,vf1,vf3
		vadd vf2,vf2,vf4
		sqc2 vf2,0(t3)
	"					: 
						: "r"(f), "r" (V1.Values), "r" (V2.Values),"r" (V3.Values)
						: "t1", "t2");
#endif
}
//------------------------------------------------------------------------------
float DotDifference(const Vector3 &D,const Vector3 &X1,const Vector3 &X2)
{         
	float temp;
#if 1//ndef SIMD
#ifndef PLAYSTATION2
	temp=D(0)*(X1(0)-X2(0))+D(1)*(X1(1)-X2(1))+D(2)*(X1(2)-X2(2));
#else
		static float d[4];
		asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v1's float address into vf1
			lqc2 vf03,0(%3)		// Load v1's float address into vf1
		// Multiply them together, and store the result in vf01
			vsub vf1,vf1,vf2
			vmul vf03,vf03,vf01
			vaddz vf04,vf03,vf03z
			vaddy vf05,vf04,vf03y
			sqc2 vf05,0(%0)
			.set reorder
		"					: /* Outputs. */
							: /* Inputs */  "r" (&d),"r" (X1.Values), "r" (X2.Values), "r" (D.Values)
							: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
		return d[0];
#endif
#else
	__asm
	{
	// Load X1's float address into eax
		mov eax,X1
	// Load X2's float address into ebx
		mov ebx,X2    
	// Load D's float address into ebx
		mov ecx,D
	// Load v1's data into xmm0
		movaps xmm0,[eax]     
		subps xmm0,[ebx]
	// Multiply them together, and store the result in xmm0
		mulps xmm0,[ecx]
	// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movhlps xmm2,xmm0	
	// Add them together, we're only interested in the lower results
		addps xmm0,xmm2
	// Shuffle xmm0's second value into xmm2's first:
		pshufd	xmm2,xmm0,01010101b	
		addss xmm0,xmm2
	// Put the contents of xmm0[0] into t3.
		movss temp,xmm0
	}
#endif        
	return temp;
}
//------------------------------------------------------------------------------
void CrossProduct(Vector3 &result,const Vector3 &v1,const Vector3 &v2)
{
#ifndef SIMD
#ifndef PLAYSTATION2     
	result.Values[0]=v1.Values[1]*v2.Values[2]-v2.Values[1]*v1.Values[2];
	result.Values[1]=v1.Values[2]*v2.Values[0]-v2.Values[2]*v1.Values[0];
	result.Values[2]=v1.Values[0]*v2.Values[1]-v2.Values[0]*v1.Values[1];
#else
	//float *v=result.Values;
	asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v2's float address into vf1
			vopmula ACC,vf01,vf02
			vopmsub vf03,vf02,vf01
			add t0,zero,%0
			sqc2 vf03,0(t0)		//  and get the result from vf03
			xor t1,t1
			sw t1,12(t0)
		"					: /* Outputs */
							: /* Inputs */ "r" (result.Values), "r" (v1.Values), "r" (v2.Values)
							: /* Clobber */ "$vf1", "$vf2", "$vf3");
#endif
#else
	_asm
	{
		mov eax,v1			// Vector3's Values
		movaps xmm0,[eax]
		mov ebx,v2			// Vector3's Values
		movaps xmm1,[ebx]
		mov ecx,result		// Vector3's Values
		/*movaps xmm4,xmm1
		movaps xmm3,xmm2
		shufps xmm1,xmm1,0x9	// << 1,2,0 -> 0x9
		shufps xmm2,xmm2,0x12	// << 2,0,1 -> 0x12
		shufps xmm3,xmm3,0x9	// << 1,2,0 -> 0x9
		shufps xmm4,xmm4,0x12	// << 2,0,1 -> 0x12
		mulps xmm1,xmm2
		mulps xmm3,xmm4
		subps xmm1,xmm3*/
		pshufd xmm2, xmm0, 0C9h
		mulps xmm2, xmm1
		pshufd xmm3, xmm1, 0C9h
		mulps xmm0, xmm3
		subps xmm0, xmm2
		pshufd xmm0, xmm0, 0c9h 
		movaps [ecx],xmm0
	}
#endif
}

 float DotProduct3(const Vector3 &v1,const Vector3 &v2)
	{
	#ifndef SIMD
	#ifndef PLAYSTATION2
		float total=0.0f;
		for(unsigned i=0;i<3;i++)
			total+=v1[i]*v2[i];
		return total;
	#else
		float d;
		asm __volatile__("  
			.set noreorder
			lwc1		$f1, 0(%1)			// load v1[X]
			lwc1		$f2, 0(%2)			// load v2[X]
			lwc1		$f3, 4(%1)			// load v1[Y]
			lwc1		$f4, 4(%2)			// load v2[Y]
			lwc1		$f5, 8(%1)			// load v1[Z]
			lwc1		$f6, 8(%2)			// load v2[Z]
			mula.s	$f1, $f2				// ACC = v1[X] * v2[X]
			madda.s	$f3, $f4				// ACC += v1[Y] * v2[Y]
			madd.s	%0, $f5, $f6		// rv = ACC + v1[Z] * v2[Z]
		"					: "=f" (d)
							:"r" (v1.Values), "r" (v2.Values)
							: );
		return d;
	#endif
	#else
		/*float temp;
			__asm
			{
			// Load v1's float address into eax
				mov eax,v1//t1
				mov eax,[eax+4]
			// Load v2's float address into ebx
				mov ebx,v2//t2   
				mov ebx,[ebx+4]
				movss xmm0,[eax]
				movss xmm1,[eax+4]
				movss xmm2,[eax+8]
				mulss xmm0,[ebx]
				mulss xmm1,[ebx+4]
				mulss xmm2,[ebx+8]
			// Multiply with v2's data, and store the result in xmm0
				addss xmm0,xmm1
				addss xmm0,xmm2
			// Put the contents of xmm0[0] back into v1.
				movss temp,xmm0
			}
			return temp;*/
			float temp;
			__asm
			{
			// Load v1's float address into eax
				mov eax,v1
			// Load v2's float address into ebx
				mov ebx,v2 
			// Load v1's data into xmm0
				movaps xmm0,[eax]
			// Multiply with v2's data, and store the result in xmm0
				mulps xmm0,[ebx]
			// Now add the four sums together:
				movhlps xmm1,xmm0
				addps xmm0,xmm1         // = X+Z,Y+W,--,--
				movaps xmm1,xmm0
				shufps xmm1,xmm1,1      // Y+W,--,--,--
				addss xmm0,xmm1
			// Put the contents of xmm0[0] back into v1.
				movss temp,xmm0
			}
			return temp;
	#endif
	}
//------------------------------------------------------------------------------
void CrossProduct(Vector3 &result,const VirtualVector &v1,const VirtualVector &v2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	result.Values[0]=v1.Values[1]*v2.Values[2]-v2.Values[1]*v1.Values[2];
	result.Values[1]=v1.Values[2]*v2.Values[0]-v2.Values[2]*v1.Values[0];
	result.Values[2]=v1.Values[0]*v2.Values[1]-v2.Values[0]*v1.Values[1];
#else
	//float *v=result.Values;
	asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v2's float address into vf1
			vopmula ACC,vf01,vf02
			vopmsub vf03,vf02,vf01
			add t0,zero,%0
			sqc2 vf03,0(t0)		//  and get the result from vf03
			xor t1,t1
			sw t1,12(t0)
		"					: /* Outputs. */ 
							: /* Inputs */ "r" (result.Values), "r" (v1.Values), "r" (v2.Values)
							: /* Clobber */ "$vf1", "$vf2", "$vf3");
#endif
#else
	_asm
	{
		mov esi,v1			// Vector3's Values
		mov eax,[esi+4]		// Put Vector3's Values in ebx
		mov esi,v2			// Vector3's Values
		mov ebx,[esi+4]		// Put Vector3's Values in ebx
		mov ecx,result		// Vector3's Values
		movaps xmm1,[eax]
		movaps xmm4,xmm1
		movaps xmm2,[ebx]
		movaps xmm3,xmm2
		shufps xmm1,xmm1,0x9	// << 1,2,0 -> 0x9
		shufps xmm2,xmm2,0x12	// << 2,0,1 -> 0x12
		shufps xmm3,xmm3,0x9	// << 1,2,0 -> 0x9
		shufps xmm4,xmm4,0x12	// << 2,0,1 -> 0x12
		mulps xmm1,xmm2
		mulps xmm3,xmm4
		subps xmm1,xmm3
		movaps [ecx],xmm1
	}
#endif
}
//------------------------------------------------------------------------------
void AddCrossProduct(Vector3 &result,const Vector3 &V1,const Vector3 &V2)
{
#ifdef SIMD
	_asm
	{
		mov eax,V1			// Vector3's Values
		movaps xmm1,[eax]
		mov ebx,V2			// Vector3's Values
		movaps xmm2,[ebx]
		mov ecx,result		// Vector3's Values
		movaps xmm5,[ecx]
		movaps xmm4,xmm1
		movaps xmm3,xmm2
		shufps xmm1,xmm1,0x9	// << 1,2,0 -> 0x9
		shufps xmm2,xmm2,0x12	// << 2,0,1 -> 0x12
		shufps xmm3,xmm3,0x9	// << 1,2,0 -> 0x9
		shufps xmm4,xmm4,0x12	// << 2,0,1 -> 0x12
		mulps xmm1,xmm2
		mulps xmm3,xmm4
		subps xmm1,xmm3
		addps xmm1,xmm5
		movaps [ecx],xmm1
	}               
#else
#ifndef PLAYSTATION2
	result.Values[0]+=V1.Values[1]*V2.Values[2]-V2.Values[1]*V1.Values[2];
	result.Values[1]+=V1.Values[2]*V2.Values[0]-V2.Values[2]*V1.Values[0];
	result.Values[2]+=V1.Values[0]*V2.Values[1]-V2.Values[0]*V1.Values[1];
#else
	//float *v=result.Values;
	asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v2's float address into vf1
			lqc2 vf04,0(%0)		// Load v2's float address into vf1
			vopmula ACC,vf01,vf02
			vopmsub vf03,vf02,vf01
			vadd vf04,vf04,vf03
			sqc2 vf04,0(%0)		//  and get the result from vf03
			xor t1,t1
			sw t1,12(%0)		// Zero the 4th number
		"					: /* Outputs. */
							: /* Inputs */ "r" (result.Values),"r" (V1.Values), "r" (V2.Values)
							: /* Clobber */ "$vf1", "$vf2", "$vf3");
#endif
#endif
//	result.CheckEdges();
}
//------------------------------------------------------------------------------
void AddDoubleCross(Vector3 &result,const Vector3 &V1,const Vector3 &V2)
{
	float a0,a1,a2;
	a0=V1.Values[1]*V2.Values[2]-V2.Values[1]*V1.Values[2];
	a1=V1.Values[2]*V2.Values[0]-V2.Values[2]*V1.Values[0];
	a2=V1.Values[0]*V2.Values[1]-V2.Values[0]*V1.Values[1];
	result.Values[0]+=V1.Values[1]*a2-a1*V1.Values[2];
	result.Values[1]+=V1.Values[2]*a0-a2*V1.Values[0];
	result.Values[2]+=V1.Values[0]*a1-a0*V1.Values[1];
}     
/// Intercept of a line on a surface.
void Intercept(Vector3 &i,const Vector3 &surf_x,const Vector3 &surf_n,
	const Vector3 &line_x,const Vector3 &line_d)
{
	float a;
	if(fabs(surf_n*line_d)<1e-9f)
		return;
	a=surf_x*surf_n;
	a-=line_x*surf_n;
	a/=(surf_n*line_d);
	i=line_x;
	AddFloatTimesVector(i,a,line_d);
}
//------------------------------------------------------------------------------
bool FiniteLineIntercept(Vector3 &i,const Vector3 &surf_x,const Vector3 &surf_n,
	const Vector3 &line_x,const Vector3 &line_d)
{
	float k=surf_n*line_d;
	if(fabs(k)<1e-9f)
		return false;
	float a=surf_x*surf_n;
	a-=line_x*surf_n;
	a/=k;
	if(a>1||a<0)
		return false;
	i=line_x;
	AddFloatTimesVector(i,a,line_d);
	return true;
}            
//------------------------------------------------------------------------------
bool FiniteLineInterceptInclusive(Vector3 &i,const Vector3 &surf_x,const Vector3 &surf_n,
	const Vector3 &line_x,const Vector3 &line_d,float Tolerance)
{
	float k=surf_n*line_d;
	if(fabs(k)<1e-9f)
		return false;
	float a=surf_x*surf_n;
	a-=line_x*surf_n;
	a/=k;
	Tolerance/=line_d.Magnitude();
	if(a>1+Tolerance||a<-Tolerance)
		return false;
	i=line_x;
	AddFloatTimesVector(i,a,line_d);
	return true;
}
//------------------------------------------------------------------------------
bool FiniteLineIntercept(Vector3 &i,const Vector3 &surf_x,const Vector3 &surf_n,
	const Vector3 &line_x,const float l,const Vector3 &line_d)
{
#ifndef PLAYSTATION2
	float k=surf_n*line_d;
	if(Fabs(k)<1e-9f)
		return false;
	float a=DotDifference(surf_n,surf_x,line_x);
	a/=k;
	if(a>l||a<0)
		return false;
	AddFloatTimesVector(i,line_x,a,line_d);
	return true;
#else
	static float A[4];
	asm __volatile__("  
		.set noreorder   
		lqc2 vf1,0(%1)
		lqc2 vf2,0(%2)
		lqc2 vf3,0(%3)
		vsub vf4,vf1,vf3
		vmul vf5,vf4,vf2			// multiples
		vaddw vf6,vf5,vf5w
		vaddz vf6,vf6,vf5z
		vaddy vf6,vf6,vf5y         	// dot product in x
		sqc2 vf6,0(%6)				// put in A
		lwc1 $f12,0(%6)				// load v1[X]

		lwc1 $f1,0(%2)				// load v1[X]
		lwc1 $f3,4(%2)				// load v1[Y]
		lwc1 $f5,8(%2)				// load v1[Z]
		lwc1 $f2,0(%4)				// load v2[X]
		lwc1 $f4,4(%4)				// load v2[Y]
		lwc1 $f6,8(%4)				// load v2[Z]
		mula.s $f1,$f2				// ACC = v1[X] * v2[X]
		madda.s	$f3,$f4				// ACC += v1[Y] * v2[Y]
		madd.s $f7,$f5,$f6			// a=f7 = ACC + v1[Z] * v2[Z]

		addi t1,zero,1
		mtc1 t1,$f3
		addi t1,zero,10000
		mtc1 t1,$f4
		cvt.s.w $f1,$f3
		cvt.s.w $f2,$f4
		
		div.s $f10,$f1,$f2			// f1 = 1e-9
		
		mfc1 t1,$f10				// = Hex
		abs.s $f8,$f7
		c.le.s $f8,$f10
		bc1t ReturnFalse			// if f7 is very small:
		nop

		div.s $f8,$f12,$f7      
		c.lt.s $f8,$f0
		bc1t ReturnFalse
		nop    
		lwc1 $f12,0(%5)
		c.lt.s $f12,$f8
		bc1t ReturnFalse
		nop
//		swc1 $f8,0(%5)				// store a

		mfc1 t1,$f8
		ctc2 t1,$vi21               // put a in the immediate I
		lqc2 vf4,0(%4)
		vmuli vf5,vf4,I				// line_d*a
		vadd vf5,vf3,vf5			// line_x+line_d*a
		sqc2 vf5,0(%0)              // store result in i
		b ReturnTrue
		nop
		ReturnFalse:
	"					: 
						: "r"(i.Values),"r"(surf_x.Values),"r"(surf_n.Values),
							"r"(line_x.Values),"r"(line_d.Values),"r"(&l),"r"(A)
						:);
	return false;
	asm __volatile__("     
		ReturnTrue:
		": 	: 	:);
	return true;
#endif
}
//------------------------------------------------------------------------------
float LineInterceptPosition(Vector3 &i,const Vector3 &surf_x,
	const Vector3 &surf_n,const Vector3 &line_x,const Vector3 &line_d)
{
#ifndef PLAYSTATION2
	float a;
	if(fabs(surf_n*line_d)<1e-9)
	{
		return 1e10f;
	}
	a=surf_x*surf_n;
	a-=line_x*surf_n;
	a/=(surf_n*line_d);
	i=line_x;
	AddFloatTimesVector(i,a,line_d);
	return a;
#else
	static float A[4];
	float *X2=surf_x.Values;
	float *X3=surf_n.Values;
	float *X4=line_x.Values;
	float *X5=line_d.Values;
	asm __volatile__("  
		.set noreorder   
		lqc2 vf1,0(%1)
		lqc2 vf2,0(%2)
		lqc2 vf3,0(%3)
		vsub vf4,vf1,vf3
		vmul vf5,vf4,vf2			// multiples
		vaddw vf6,vf5,vf5w
		vaddz vf6,vf6,vf5z
		vaddy vf6,vf6,vf5y         	// dot product in x
		sqc2 vf6,0(%5)				// put in A
		lwc1 $f12,0(%5)				// load v1[X]

		lwc1 $f1,0(%2)				// load v1[X]
		lwc1 $f3,4(%2)				// load v1[Y]
		lwc1 $f5,8(%2)				// load v1[Z]
		lwc1 $f2,0(%4)				// load v2[X]
		lwc1 $f4,4(%4)				// load v2[Y]
		lwc1 $f6,8(%4)				// load v2[Z]
		mula.s $f1,$f2				// ACC = v1[X] * v2[X]
		madda.s	$f3,$f4				// ACC += v1[Y] * v2[Y]
		madd.s $f7,$f5,$f6			// a=f7 = ACC + v1[Z] * v2[Z]
		
		addi t1,zero,1
		mtc1 t1,$f3
		addi t1,zero,10000
		mtc1 t1,$f4
		cvt.s.w $f1,$f3
		cvt.s.w $f2,$f4
		
		div.s $f10,$f1,$f2			// f1 = 1e-9
		
		mfc1 t1,$f10					// = Hex
		abs.s $f8,$f7
		c.le.s $f8,$f10
		bc1t Escape					// if f7 is very small:
		nop

		div.s $f8,$f12,$f7      
		swc1 $f8,0(%5)				// store a

		mfc1 t1,$f8
		ctc2 t1,$vi21               // put a in the immediate I
		lqc2 vf4,0(%4)
		vmuli vf5,vf4,I				// line_d*a
		vadd vf5,vf3,vf5			// line_x+line_d*a
		sqc2 vf5,0(%0)              // store result in i
		Escape:
	"					: 
						: "r"(i.Values),"r"(X2),"r"(X3),"r"(X4),"r"(X5),"r"(A)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
	return A[0];
#endif
}
//------------------------------------------------------------------------------
void InsertVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;i++)
		M.Values[row*M.W16+i]=v(i);
#else
	asm __volatile__("
		.set noreorder
		add t0,zero,%3
		beqz t0,Escape
		add t2,zero,%1
		lw t1,0(a0)
		add t3,zero,a2	//t3=row
		lw t4,8(%0)		// t4=M.W16
		muli t4,t4,4
		mul t4,t4,t3	// row*MStride
		add t1,t4
		Loop:
			lqc2 vf1,0(t2)
			sqc2 vf1,0(t1)
			addi t2,16
			addi t0,-4
		bgtz t0,Loop
			addi t1,16
		Escape:
		.set reorder
	"					  : 
						  :  "r" (&M), "r"(v.Values),"r" (row),"r"(v.size)
						  :);
#endif
#else
	_asm
	{
		mov ebx,v			// Vector3's address
		movaps xmm0,[ebx]
		mov edi,M			// Matrix's Values
		mov ecx,[edi]
		mov eax,[edi+8]		// Put W16 into eax
		shl eax,2			// Multiply by 4 to get the matrix stride
		mov edx,row			// Address of row
		mul edx				// Shift in address from the top to the row
		add ecx,eax			// Put the row's address in ecx
		movaps [ecx],xmm0
	}
#endif
}
void Multiply(Vector3 &V2,const Matrix &M,const Vector3 &V1)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Width!=4)
		throw Vector3::BadSize();
	if(M.Height>4)
		throw Vector3::BadSize();
#endif
#ifdef SIMD
	unsigned MStride=M.W16*4;
	unsigned W=M.Width;
	float *Result=(float *)(V2.Values);
	float *v1=(float *)(V1.Values);
	static unsigned t1[]={-1,0,0,0};
	static unsigned t2[]={-1,-1,0,0};
	static unsigned t3[]={-1,-1,-1,0};
	static unsigned t4[]={-1,-1,-1,-1};   
	unsigned *tt1=t1;
	unsigned *tt2=t2;
	unsigned *tt3=t3;
	unsigned *tt4=t4;   
	unsigned *tt=NULL;
	_asm
	{          
		mov ecx,W  
		cmp ecx,0
		je Escape
		and ecx,3
		cmp ecx,0
		je Mask4  
		cmp ecx,3
		je Mask3
		cmp ecx,2
		je Mask2
		//Mask1:
			mov eax,tt1
			jmp EndMask
		Mask2:
			mov eax,tt2  
			jmp EndMask
		Mask3:
			mov eax,tt3  
			jmp EndMask
		Mask4:
			mov eax,tt4
		EndMask:     
			mov tt,eax
		mov ecx,M
		mov edx,[ecx+12]
		cmp edx,0
		je Escape
		mov edi,dword ptr[Result]
		mov esi,dword ptr[ecx]
		Row_Loop:
			mov ebx,dword ptr[v1]
			mov eax,esi
			xorps xmm6,xmm6
			movaps xmm1,[eax]
			movaps xmm2,[ebx]
			mov ecx,tt
			movups xmm7,[ecx]
			andps xmm2,xmm7   
			mulps xmm1,xmm2
			addps xmm6,xmm1
			shufps xmm1,xmm6,0xEE
			shufps xmm1,xmm1,0xEE
			addps xmm6,xmm1
			shufps xmm1,xmm6,0x11
			shufps xmm1,xmm1,0xEE
			addss xmm6,xmm1
			movss [edi],xmm6
			Add edi,4
			mov ecx,MStride
			add esi,ecx
			dec edx
			cmp edx,0
		jne Row_Loop
		Escape:
	}
#else
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		V2.Values[i]=0.f;
		for(j=0;j<M.Width;j++)  // j=1,2,(4),17
		{
			if(j<3)
				V2.Values[i]+=M(i,j)*V1.Values[j];
			else if(j==3)
				V2.Values[i]+=M(i,j);
		}
	}   
#else
	float *Row1=(M.Values);
	float *Row2=(V1.Values);
	unsigned W=M.Width;
	unsigned W16=(M.Width+3)&~0x3;
	unsigned H=V2.size;
	unsigned Stride=M.W16*4;
	asm __volatile__("
		.set noreorder  
		add t4,zero,%4
		blez t4,Escape	// for zero height matrix 
		xor t5,t5						// V2's value count
		add t9,zero,%0						// Start of V2.Values
		iLoop:							// Rows of M1 and M
			add t0,zero,t9				// Start of Row i of M
			add t7,zero,%2 				// Row zero of M2
			jLoop:
				add t1,zero,%1			// Start of Row i of M1
				add t3,zero,%3			// Width of M1 and M2
				add t2,zero,t7 			// Row j of M2
				sub.s $f4,$f0,$f0		// f1=0
				blt t3,4,Tail
				vsub vf4,vf0,vf0
				kLoop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmul vf3,vf1,vf2
					vadd vf4,vf4,vf3
					addi t1,16			// next four numbers
					addi t3,-4			// counter-=4
				bgt t3,3,kLoop
					addi t2,16
// Now vf4 contains 4 sums to be added together.
				vaddx vf5,vf4,vf4x
				vaddy vf5,vf4,vf5y
				vaddz vf5,vf4,vf5z  	// So the sum is in the w.
				vmul vf5,vf5,vf0		// Now it's 3 zeroes and the sum in the w slot.
// Rotate into place.
				and t6,t5,0x3			//	where is the column in 4-aligned data?
				and t8,t0,~0xF			//	4-align the pointer
				beq t6,3,esccase		// W
				nop
				beq t6,2,esccase		// Z
					vmr32 vf5,vf5
				beq t6,1,esccase		// Y
					vmr32 vf5,vf5
				b emptycase				// just store, don't add to what's there.
					vmr32 vf5,vf5		// X
			esccase:
				lqc2 vf6,0(t8)
				vadd vf5,vf6,vf5		// add to what's there already.
			emptycase:
				sqc2 vf5,0(t8)			// store in t0
				lwc1 $f4,0(t0)			// Put the sum in f4
			blez t3,NoTail
			Tail:
			TailLoop:
				lwc1 $f1,0(t1)		// Load 4 numbers into vf1
				lwc1 $f2,0(t2)		// Load 4 numbers into vf2
				mul.s $f3,$f1,$f2
				add.s $f4,$f4,$f3
				addi t1,4			// next four numbers
				addi t3,-1			// counter-=4
			bgtz t3,TailLoop
				addi t2,4
			swc1 $f4,0(t0)
			NoTail:
			addi t9,4					// Next Value of V2
			add %1,%1,%5				// Next Row of M1
			addi t4,-1					// row counter
			addi t5,1					// where in 4-byte aligned V2 vector
		bgtz t4,iLoop
		nop
		Escape:
	"					: 
						: "r" (V2.Values),"r" (Row1), "r" (Row2), "r" (W), "r" (H), "r" (Stride)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#endif
}
//------------------------------------------------------------------------------  
#include "Matrix4x4.h"
void ColumnToVector(const Matrix4x4 &M,Vector3 &V,unsigned Col)
{
	for(unsigned i=0;i<4;i++)
		V.Values[i]=M.Values[i*4+Col];
}
//------------------------------------------------------------------------------  
void RowToVector(const Matrix4x4 &M,Vector3 &V,unsigned Row)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<4;i++)
		V.Values[i]=M.Values[Row*4+i];
#else
	float *RowPtr1=&(M.Values[Row*M.W16]);
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape				// for zero width matrix 
		ble t0,3,Tail
		nop
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			sqc2 vf1,0(t2)          // Load 4 numbers into vector
			addi t1,16				// next four numbers
			addi t0,-4				// counter-=4
		bgt t0,3,iLoop
			addi t2,16
		blez t0,Escape
		Tail:
		tLoop:
			addi t0,-1				// counter-=4
			lwc1 f1,0(t1)			// Load a number into f1
			swc1 f1,0(t2)
			addi t1,4				// next four numbers
		bgtz t0,tLoop
			addi t2,4
		Escape:
	"					: /* Outputs. */
						: /* Inputs */  "r" (M.Width), "r" (RowPtr1), "r" (V.Values)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else      
	unsigned Stride=4*4;
	_asm
	{
		mov edi,M
		mov esi,V
		mov eax,Row
		mul Stride
		add edi,eax
		movaps xmm2,[edi]
		movaps [esi],xmm2
	}
#endif            
}
void Multiply(Vector3 &V2,const float f,const Vector3 &V1)
{      
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;i++)
		V2.Values[i]=V1.Values[i]*f;
#else
	static float d[4];
	d[0]=f;
	asm __volatile__("
		.set noreorder  
		lqc2 vf4,0(%0)
		vsub vf3,vf00,vf00
		vaddx vf3,vf3,vf4x			// All 4 numbers are equal to multiplier
		add t0,zero,%3				// V1.size
		blez t0,Escape				// for zero width matrix 
		nop
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			vmul vf2,vf1,vf3
			sqc2 vf2,0(t2)
			addi t1,16				// next four numbers
			addi t2,16
			addi t0,-4				// counter-=4
			bgtz t0,iLoop
		nop
		Escape:
	"					: 
						: "r"(d),"r"(V1.Values),"r"(V2.Values),"r"(V1.size)
						: );
#endif
#else
	__asm
	{
		movss xmm0,f
		shufps xmm0,xmm0,0x0
		mov ebx,V1
		mov eax,V2
		mulps xmm0,[ebx]
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void Subtract(VirtualVector &R,const Vector3 &X1,const Vector3 &X2)
{
#ifndef SIMD
	R(0)=X1(0)-X2(0);
	R(1)=X1(1)-X2(1);
	R(2)=X1(2)-X2(2);
#else
	__asm
	{
		mov eax,X1
		mov ebx,X2
		mov ecx,R
		mov ecx,[ecx+4]
		movaps xmm1,[eax]
		movaps xmm2,[ebx]
		subps xmm1,xmm2
		movss [ecx],xmm1
		shufps xmm1,xmm1,9
		movss [ecx+4],xmm1
		shufps xmm1,xmm1,9
		movss [ecx+8],xmm1
	}
#endif
}
//------------------------------------------------------------------------------
void VectorOntoSurface(Vector3 &X,const Vector3 &S,const Vector3 &N)
{
	AddFloatTimesVector(X,DotDifference(N,S,X),N);
}           
//------------------------------------------------------------------------------
float HeightAboveSurface(Vector3 &X,const Vector3 &S,const Vector3 &N)
{
	return DotDifference(N,X,S);
}
//------------------------------------------------------------------------------
void VectorOntoOriginPlane(Vector3 &R,const Vector3 &N)
{
#ifndef SIMD
	AddFloatTimesVector(R,-(R*N),N);
#else
	__asm
	{
			mov eax,R
			movaps xmm4,[eax]
			movaps xmm0,xmm4
			mov ecx,N
			mulps xmm0,[ecx]	// xmm0=RxNx, RyNy,RzNz


		movaps xmm2,xmm0
		shufps xmm0,xmm0,9	// xmm0= RyNy, RzNz,..
		addps xmm0,xmm2		// xmm0= RxNx + RyNy
		shufps xmm2, xmm2,18// xmm2= RzNz, ..
		addss xmm0,xmm2		// xmm0.x = dist squared.

		shufps xmm0,xmm0,0
		movaps xmm1,[ecx]
		mulps xmm0,xmm1
		subps xmm4,xmm0
		movaps [eax],xmm4
	}
#endif
}
bool DistanceCheck(const Vector3 &X1,const Vector3 &X2,float R)
{
#ifndef SIMD
	Vector3 TempVector3;
	Subtract(TempVector3,X2,X1);
	return(!(TempVector3>R));
#else
	_asm
	{
		mov ebx,X1
		movaps xmm0,[ebx]
		mov ecx,X2
		subps xmm0,[ecx]	// Issue 2, latency 4.
		
		movss xmm1,R		// R in xmm1.0
		shufps xmm1,xmm1,0	// xmm1 ={R,R,R,R}
		mov eax,0			// Return false if any of the following tests fails:
		
		cmpleps xmm1,xmm0	// if any xmm0 value is greater than the equivalent from xmm1
		movmskps ebx,xmm1	// Move result into eax
		cmp ebx,0
		jne return_ea		// Any result, return false, the vectors are too far away.

		mulps xmm0,xmm0		// X squared, Y squared, Z squared		Issue 2, Latency 5!

		movss xmm1,R		// R in xmm1.0
		mulss xmm1,xmm1		// R squared

		movaps xmm2,xmm0
		shufps xmm0,xmm0,9
		addps xmm0,xmm2
		shufps xmm2, xmm2,18
		addss xmm0,xmm2		// xmm0.x = dist squared.

		comiss xmm0,xmm1	// Compare R to dist
		jnbe return_ea		// Must use jnbe not jg! Why?
		mov eax,1
return_ea:
	}
#endif
}
void MultiplyElements(Vector3 &ret,const Vector3 &V1,const Vector3 &V2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		ret.Values[i]=V1.Values[i]*V2.Values[i];
#else
	ret;
	V1;
	V2;
	asm __volatile__("  
		.set noreorder
		Loop:
			lqc2 vf01,0(%1)
			lqc2 vf02,0(%2)
			vmul vf03,vf01,vf02
			sqc2 vf03,0(%0)
			addi %0,16
			addi %1,16
			addi %3,-4
		bgtz %3,Loop
			addi %2,16
	"					: /* Outputs. */ 
						: "r"(ret.Values),"r"(V1.Values),"r"(V2.Values),"r"(V1.size)
						: /* Clobber */);
#endif
#else
	for(unsigned i=0;i<3;i++)
		ret.Values[i]=V1.Values[i]*V2.Values[i];
#endif
}        
void Vector3::CheckNAN() const
{
	for(unsigned i=0;i<3;i++)
	{
#ifdef WIN32
		if(_isnan(Values[i]))
#else
	#if defined(SN_TARGET_PS3) || defined(__GNUC__)
		if(isnan(Values[i]))
	#else
		if(_isnan(Values[i]))
	#endif
#endif
		{
#ifdef SIMUL_EXCEPTIONS
			throw BadNumber();
#endif
		}
	}
}
Vector3 Max(const Vector3 &v1,const Vector3 &v2)
{
	return Vector3(std::max(v1.x,v2.x),std::max(v1.y,v2.y),std::max(v1.z,v2.z));
}
Vector3 Min(const Vector3 &v1,const Vector3 &v2)
{
	return Vector3(std::min(v1.x,v2.x),std::min(v1.y,v2.y),std::min(v1.z,v2.z));
}
Vector3 Lerp(float x,const Vector3 &v1,const Vector3 &v2)
{
	Vector3 ret=v1;
	ret*=(1.f-x);
	AddFloatTimesVector(ret,x,v2);
	return ret;
}
Vector3 CatmullRom(float t,const Vector3 &x0,const Vector3 &x1,const Vector3 &x2,const Vector3 &x3)
{
	Vector3 X=2.f*x1;
	AddFloatTimesVector(X,t,(x2-x0));
	AddFloatTimesVector(X,t*t,(2.f*x0-5.f*x1+4.f*x2-x3));
	AddFloatTimesVector(X,t*t*t,(-x0+3.f*x1-3.f*x2+x3));
	X*=.5f;
	return X;
}
	
std::ostream &operator<<(std::ostream &out, Vector3 const &)
{
	//out.write((const char*)(I.FloatPointer(0)),sizeof(Vector3));
	return out;
}
std::istream &operator>>(std::istream &in, Vector3 &)
{
	//in.read((char*)(I.FloatPointer(0)),sizeof(Vector3));
	return in;
}
#ifndef VECTOR3_INL
#define VECTOR3_INL

/// Let R equal X1 plus X2.
 void Add(Vector3 &R,const Vector3 &X1,const Vector3 &X2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]+X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vadd vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov eax,X1 
		movaps xmm0,[eax]
		mov ecx,X2
		addps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
/// Let R equal X1 minus X2.
 void Subtract(Vector3 &R,const Vector3 &X1,const Vector3 &X2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<3;i++)
		R.Values[i]=X1.Values[i]-X2.Values[i];
#else
	X1;X2;R;
	asm __volatile__("
		.set noreorder   
		blez %3,Escape 
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add t3,zero,%3
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vsub vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	__asm
	{
		mov eax,X1  
		movaps xmm0,[eax]
		mov ecx,X2
		subps xmm0,[ecx]
		mov eax,R
		movaps [eax],xmm0
	}
#endif
}
 void Reflect(Vector3 &R,const Vector3 &N,const Vector3 &D)
{
	float along=N*D;
	R=D;
	AddFloatTimesVector(R,-2*along,N);
}
#endif

 void ClosestApproach(	float &alpha1,float &alpha2,
						 const Vector3 &X1,const Vector3 &D1,
						 const Vector3 &X2,const Vector3 &D2)
 {
	Vector3 DX;
	Subtract(DX,X1,X2);
	float d1d2=D1*D2;
	float d11=(D1*D1);
	float d22=(D2*D2);
	float d1d2_1=(d11*d22-(d1d2*d1d2));// in case of scaling!
	if(d1d2_1>1e-4f)
	{
		alpha1=1/d1d2_1*(-d22*(D1*DX)+d1d2*(D2*DX));
		alpha2=1/d1d2_1*(-d1d2*(D1*DX)+d11*(D2*DX));
	}
	else
	{
		if(d1d2>=1.f)
		{
			alpha1=1e23f;
			alpha2=-1e23f;
		}
		else
		{
			alpha1=-1e23f;
			alpha2=1e23f;
		}
	}
}
}
}
