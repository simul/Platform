#define SIM_MATH
#include "SimVector.h"
#include "Vector3.h"
#include <math.h>     
#include <algorithm>
#include <assert.h>

#ifdef WIN32
	#pragma pack(16)
#endif

#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#else
#undef CHECK_MATRIX_BOUNDS
#endif

#ifdef WIN64
	typedef unsigned __int64 ptr;
#else
	#ifdef __LP64__
		#include <stdint.h>
		typedef uint64_t ptr;
	#else
		typedef unsigned long long ptr;
	#endif
#endif

namespace simul
{
	namespace math
	{
Vector::Vector(void)
{
	size=0;
	S16=0;
	Values=NULL;
	V=NULL;
}         
//------------------------------------------------------------------------------
Vector::Vector(const Vector &v)
{
	{
		size=v.size;
	// Force 16-byte alignment, as required by Pentium III+ and Playstation processors
	// for vectorized floating point:
	#ifdef CANNOT_ALIGN
		S16=(((unsigned)size+3)&~0x3)+4;
	// round up to four floats, then add four as pre-padding.
		V=new float[S16];
		Values=(float*)(((ptr)V+15)&~0xf);
	// Zero the top 4 Values here, so they don't affect any results later on:
		for(unsigned i=0;i<S16;i++)
			V[i]=0;
	#else
		S16=(((unsigned)size+3)&~0x3);
		Values=new(*CurrentMemoryPool,16) float[S16];
		for(unsigned i=size;i<S16;i++)
			Values[i]=0;   
	#endif
		for(unsigned i=0;i<size;i++)
			Values[i]=v.Values[i];
	}      
}
//------------------------------------------------------------------------------
Vector::Vector(unsigned psize)
{
	size=psize;
#ifdef CANNOT_ALIGN
	S16=(((unsigned)size+3)&~0x3)+4;  // round up to four floats, then add four as pre-padding.
	V=new float[S16];
	Values=(float*)(((ptr)V+15)&~0xf);
	unsigned i;
	for(i=0;i<S16;i++)
		V[i]=0;
#else
	S16=(((unsigned)size+3)&~0x3);
	Values=new(*CurrentMemoryPool,16) float[S16];
	unsigned i;
	for(i=0;i<S16;i++)
		Values[i]=0;
#endif
}
//------------------------------------------------------------------------------
Vector::Vector(float x1,float x2,float x3)
{
	size=3;
#ifdef CANNOT_ALIGN
	S16=(((unsigned)size+3)&~0x3)+4;  // round up to four floats, then add four as pre-padding.
	V=new float[S16];
	Values=(float*)(((ptr)V+15)&~0xf);
	unsigned i;
	for(i=0;i<S16;i++)
		V[i]=0;
#else
	S16=(((unsigned)size+3)&~0x3);
	Values=new(*CurrentMemoryPool,16) float[S16];
	unsigned i;
	for(i=0;i<S16;i++)
		Values[i]=0;
#endif
	Values[0]=x1;
	Values[1]=x2;
	Values[2]=x3;
}
//------------------------------------------------------------------------------
Vector::Vector(const float *x)
{
	size=3;
#ifdef CANNOT_ALIGN
	S16=8;
	V=new float[S16];
	Values=(float*)(((ptr)V+15)&~0xf);  
	V[3]=0;
	V[3+1]=0;
	V[3+2]=0;
	V[3+3]=0;  
#else
	S16=4;
	Values=new(*CurrentMemoryPool,16) float[S16];
	Values[3]=0;
#endif
	Values[0]=x[0];
	Values[1]=x[1];
	Values[2]=x[2];
}
//------------------------------------------------------------------------------
void Vector::Resize(unsigned s)
{
	if(size!=s)
		ChangeSize(s); 
}
//------------------------------------------------------------------------------
void Vector::ChangeSize(unsigned s)
{          
	size=s;
#ifdef CANNOT_ALIGN   
	if(((size+3)&~0x3)+4<=S16)
	{                       
#if 1//ndef SIMD
		unsigned i;
		for(i=size;i<((size+3)&~0x3);i++)
			Values[i]=0;
#else
		_asm
		{
			mov ecx,this
			mov ebx,[ecx+4]
			mov eax,[ecx+8]
			imul eax,4
			and eax,0xFFFFFFF0
			add ebx,eax
			xorps xmm1,xmm1
			movaps [ebx],xmm1
		}
#endif
		return;
	}
	delete[] V;
	S16=(((unsigned)s+3)&~0x3)+4;  // round up to four floats, then add four as pre-padding.
	V=new float[S16];
	Values=(float*)(((ptr)V+15)&~0xf);
	unsigned i;
	for(i=0;i<S16;i++)
		V[i]=0;
#else             
	if((((unsigned)size+3)&~0x3)<=S16)
	{
#ifndef PLAYSTATION2
		for(unsigned i=size;i<(((unsigned)size+3)&~0x3);i++)
			Values[i]=0;
#else
		if(!size)
			return;
		asm __volatile__("
			.set noreorder
			//add t0,%1,3
			and t0,%1,~0x3
			bge t0,%2,Escape
			muli t0,t0,4
			blez t0,Escape
			add t0,%0,t0
			and t0,t0,0xFFFFFFF0
			vsub vf1,vf0,vf0
			sqc2 vf1,0(t0)
			Escape:
			.set reorder
		"					: 
							: "r" (Values),"r"(size),"r"(S16)
							: );
#endif
		return;
	}
	delete[] Values,*CurrentMemoryPool,16);
	S16=(((unsigned)s+3)&~0x3);
	Values=new(*CurrentMemoryPool,16) float[S16];
	for(unsigned i=size;i<(((unsigned)size+3)&~0x3);i++)
		Values[i]=0;
#endif
}
//------------------------------------------------------------------------------
Vector::~Vector(void)
{              
#ifdef CANNOT_ALIGN
	delete[] V;
#else 
	delete[] Values,*CurrentMemoryPool,16);
#endif
}
//------------------------------------------------------------------------------
void Vector::ClearMemory(void)
{       
#ifdef CANNOT_ALIGN
	delete[] V;
	V=NULL;
#else
	delete[] Values,*CurrentMemoryPool,16);
	Values=NULL;
#endif
	size=0;
	S16=0;
}
//------------------------------------------------------------------------------
/*void Vector::Define(float x,float y,float z)
{
	Values[0]=x;
	Values[1]=y;
	Values[2]=z;
}                  
//------------------------------------------------------------------------------
void Vector::DefineValues(float *x)
{
	Values[0]=x[0];
	Values[1]=x[1];
	Values[2]=x[2];
}*/
//------------------------------------------------------------------------------
bool Vector::NonZero(void)
{
	for(unsigned i=0;i<size;i++)
	{
		if(Values[i]*Values[i]>1e-16f)
			return true;
	}
	return false;
}           
float &Vector::operator[](unsigned index)
{                       
//	CheckEdges();
	return Values[index];
}                       
float Vector::operator[](unsigned index) const
{                      
//	CheckEdges();
	return Values[index];
}              
float &Vector::operator() (unsigned index)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(index>=size)
	{
		unsigned errorhere=0;
		errorhere++;
		errorhere;
		throw BadSize();
	}
#endif
	return Values[index];
}
/// The float at position given by index.
float Vector::operator() (unsigned index) const
{                
#ifdef CHECK_MATRIX_BOUNDS
	if(index>=size)
	{
		unsigned errorhere=0;
		errorhere++;
		errorhere;
		throw BadSize();
	}              
#endif             
//	CheckEdges();
	return Values[index];
}

float * Vector::Position(unsigned index) const
{
	return &(Values[index]);
}
float Vector::Magnitude(void) const
{           
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i;
	float total=0.0f;
	for(i=0;i<size;i++)
	{
		total+=Values[i]*Values[i];
	}
	return sqrt(total);
#else
	if(size<=4)
	{
		float *t1=Values;
		static float temp;
		static float *t3=&temp;
		__asm
		{
		// Load v1's float address into eax
			mov eax,t1
		// Load the return float address into ecx
			mov ecx,t3
		// Load v1's data into xmm0
			movaps xmm0,[eax]
		// MKultiply them together, and store the result in xmm0
			mulps xmm0,xmm0
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
			shufps xmm2,xmm0,0xBB
			shufps xmm2,xmm2,0xBB
		// Add them together, we're only interested in the lower results
			addps xmm0,xmm2
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
			shufps xmm2,xmm0,0x55
			shufps xmm2,xmm2,0xBB
			addss xmm0,xmm2
			sqrtss xmm0,xmm0
		// Put the contents of xmm0[0] back into v1.
			movss [ecx],xmm0
		}
		return temp;
	}
	else
	{
		unsigned i;
		float total=0.0f;
		for(i=0;i<size;i++)
		{
			total+=Values[i]*Values[i];
		}
		return Sqrt(total);
	}
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
			sqrt.s	%0,%0
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
void Vector::CheckEdges(void) const
{       
	unsigned i;
	for(i=size;i<((size+3)&~3);i++)
	{
		 if(Values[i]!=0)
		 {
#ifdef SIMUL_EXCEPTIONS
		 	throw Vector::BadSize();
#endif
		 }
	}
}
//------------------------------------------------------------------------------
void Vector::Insert(const Vector &v,unsigned pos)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(pos+v.size>size)
		throw Vector::BadSize();
#endif
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<v.size;i++)
		Values[pos+i]=v.Values[i];
#else                
	float *Row1=&(Values[pos]);
	float *Row2=v.Values;
	_asm
	{             
		mov esi,v
		mov edi,this
		mov eax,[esi+8]			// v.size
		cmp eax,0  
		jle Escape
		mov ebx,Row1
		mov ecx,Row2
		mov edx,ebx
		and edx,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		cmp edx,0
		jne uLoop			// if not, do the unaligned copy
		mov esi,[edi+8]		// size
		mov edx,pos
		add edx,eax			// pos+v.size
		cmp eax,4
		jl Unaligned
		Loop4:
			movaps xmm1,[ecx]	// Load 4 numbers from ecx
			movaps [ebx],xmm1	// Move 4 numbers into ebx
			add ebx,16			// next four numbers
			add ecx,16              
			add eax,-4			// counter-=4
			cmp eax,3
		jg Loop4
		Unaligned:
		cmp eax,0
		jle Escape				// Exactly 4 or overrun
		uLoop:
			mov edx,[ecx]		// Load 1 number into vf1
			mov [ebx],edx		// Move 1 number into vf2
			add ebx,4			// next four numbers
			add ecx,4
			add eax,-1			// counter-=4
		jg uLoop
		Escape:
	};
#endif
#else
	float *Row1=&(Values[pos]);
	asm __volatile__("
		.set noreorder
		add t1,zero,%1
		add t2,zero,%2
		add t0,zero,%0			// v.size
		blez t0,Escape
		and t3,t1,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		bnez t3,uLoop			// if not, do the unaligned copy
		add t5,zero,%3			// size
		//add t7,t0,%4			// pos+v.size
		//beq t7,t5,NoTail
		//nop
		NoTail:
		blt t0,4,Unaligned
		nop
		Loop:
			addi t0,-4			// counter-=4
			lqc2 vf2,0(t2)		// Load 4 numbers into vf1
			sqc2 vf2,0(t1)		// Load 4 numbers into vf2
			addi t1,16			// next four numbers
		bgt t0,3,Loop
			addi t2,16
		blez t0,Escape			// Exactly 4 or overrun
		Unaligned:
		uLoop:
			lwc1 f2,0(t2)		// Load 4 numbers into vf1
			swc1 f2,0(t1)		// Load 4 numbers into vf2
			addi t1,4			// next four numbers
			addi t0,-1			// counter-=4
		bgtz t0,uLoop
			addi t2,4
		Escape:
		.set reorder
	"					:
						: "r" (v.size), "r" (Row1), "r" (v.Values),"r"(size),"r"(pos)
						:  "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}
bool Vector::Unit(void)
{           
#ifndef SIMD
#ifndef PLAYSTATION2
	float sz=Magnitude();
	if(!sz)
		return false;
	for(unsigned i=0;i<size;i++)
		Values[i]/=sz;
	return true;
#else
	float *VV=Values;
	if(size<=3)
	{
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
	}
	else
	{
		float sz=Magnitude();
		if(!sz)
			return false;
		for(unsigned i=0;i<size;i++)
			Values[i]/=sz;
		return true;
	}
						
#endif
#else       
	if(size<=4)
	{
		//float *t1=Values;
		__asm
		{
		// Load v1's float address into eax
			mov eax,this
			mov eax,[eax+4]
		// Load v1's data into xmm0  
			movaps xmm1,[eax]
			movaps xmm0,xmm1
		// MKultiply them together, and store the result in xmm0
			mulps xmm0,xmm0
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
			shufps xmm2,xmm0,0xBB
			shufps xmm2,xmm2,0xBB
		// Add them together, we're only interested in the lower results
			addps xmm0,xmm2
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
			shufps xmm2,xmm0,0x55
			shufps xmm2,xmm2,0xBB
			addps xmm0,xmm2
			rsqrtss xmm0,xmm0   
			shufps xmm0,xmm0,0x0    
			mulps xmm1,xmm0
		// Put the contents of xmm0 back into v1.
			movaps [eax],xmm1
		}
	}
	else
	{
		float sz=Magnitude();
		if(!sz)
			return false;
		for(unsigned i=0;i<size;i++)
			Values[i]/=sz;
		return true;
	}       
	return true;
#endif
}
//-----------------------------------------------------------------------------
void Vector::Zero(void)
{
#ifdef SIMD
	_asm
	{
		mov ebx,this
		mov eax,[ebx+8]
		cmp eax,0
		je iEscape   
		mov esi,[ebx+4]
		xorps xmm0,xmm0
		iLoop:
			movaps [esi],xmm0
			add esi,16
			add eax,-4
			cmp eax,0
		jg iLoop   
		iEscape:
	}
#else
#ifdef PLAYSTATION2
	asm __volatile__
	(
	"
		.set noreorder
		add t0,zero,%0
		add t2,zero,%1
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
		: "r" (Values),"r"(size)
		:  );
#else
	unsigned i;
	for(i=0;i<size;++i)
		Values[i]=0.f;
#endif
#endif
}
Vector Vector::operator-()
{
	unsigned i;
	Vector temp(size);
	for(i=0;i<size;++i)
	{
		temp[i]=-Values[i];
	}
	return temp;
}
void Vector::CheckNAN(void)
{
#ifdef _DEBUG
	unsigned i;
	for(i=0;i<size;i++)
	{
#ifdef WIN32
		if(_isnan(Values[i])||!_finite(Values[i]))
#else
#ifndef _MSC_VER
		if(isnan(Values[i]))
#else
		if(_isnan(Values[i]))
#endif
#endif
		{
#ifdef SIMUL_EXCEPTIONS
			throw Vector::BadNumber();
#endif
		}
	}
#endif
}
bool Vector::ContainsNAN(void)
{
#ifdef _DEBUG
	unsigned i;
	for(i=0;i<size;i++)
	{
#ifndef _MSC_VER
		if(isnan(Values[i]))
#else
		if(_isnan(Values[i]))
#endif
			return true;
	}
#endif
	return false;
}
// Logical operators
bool operator==(Vector & v1,Vector & v2)
{
	bool result;
	unsigned i;
	result=true;
	if(v1.size!=v2.size)
		return false;
	for(i=0;i<v1.size;i++)
	{
		if(v1(i)!=v2(i))
			result=false;
	}
	return result;
}
//------------------------------------------------------------------------------
bool operator!=(Vector &v1,Vector &v2)
{
	bool result;
	unsigned i;
	result=false;
	if(v1.size!=v2.size)
		return true;
	for(i=0;i<v1.size;i++)
	{
		if(v1(i)!=v2(i))
			result=true;
	}
	return result;
}
// Calculation operators
Vector operator+(const Vector &v1,const Vector &v2)
{
	unsigned i;
	Vector v(v1.size);
	assert(v1.size==v2.size);
	for(i=0;i<v.size;i++)
	{
		v(i)=v1.Values[i]+v2.Values[i];
	}
	return v;
}
Vector operator-(const Vector &v1,const Vector &v2)
{
	unsigned i;
	Vector v(v1.size);
	if(v1.size!=v2.size)
		return v;
	for(i=0;i<v.size;i++)
		v.Values[i]=v1.Values[i]-v2.Values[i];
	return v;
}                                     
//------------------------------------------------------------------------------
void Vector::operator=(const Vector &v)
{
//	if (this == &f) return *this;   //  handle self assignment
	if(size!=v.size)
		Resize(v.size);
	operator|=(v);
}            
//------------------------------------------------------------------------------    
void Vector::operator=(const float *f)
{
	for(unsigned i=0;i<size;i++)
		Values[i]=f[i];
}
//------------------------------------------------------------------------------
void Vector::operator=(const Vector3 &v)
{
	if(size!=3)
		Resize(3);
	for(unsigned i=0;i<3;i++)
	{
		float f=v(i);
		Values[i]=f;
	}
}
//------------------------------------------------------------------------------
void Vector::operator|=(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size<v.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}           
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	unsigned s=((unsigned)size+3)&~0x3;
	for(i=0;i<s;i++)
		Values[i]=v.Values[i];
#else
	asm __volatile__("     
		.set noreorder
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		blez t0,Escape
		Loop:
			addi t0,-4			// counter-=4
			lqc2 vf2,0(t2)		// Load 4 numbers into vf1
			addi t2,16
			sqc2 vf2,0(t1)		// Load 4 numbers into vf2
		bgtz t0,Loop
			addi t1,16			// next four numbers
		Escape:
		.set reorder
	"					:
						: "r" (v.size), "r" (Values), "r" (v.Values)
						:  "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	__asm
	{
		mov edi,this
		mov esi,v
		mov esi,[esi+4]   // ebx=v.Values
		mov ecx,[edi+8]
		mov edi,[edi+4]

		rep movsd
/*
		cmp ecx,0
		je Escape
		Eq_Loop:
			movaps xmm0,[esi]
			add esi,16
			sub ecx,4
			movaps [edi],xmm0
			add edi,16
			cmp ecx,0
		jg Eq_Loop
		Escape:*/
	}
#endif
}
//------------------------------------------------------------------------------
void Subtract(Vector &R,const Vector &X1,const Vector &X2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(R.size>X1.size||R.size>X2.size)
	{
		throw Vector::BadSize();
	}           
	if(R.size==0)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<R.size;i++)
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
		Loop:
			lqc2 vf1,0(t1)
			addi t1,16
			lqc2 vf2,0(t2)
			addi t2,16
			vsub vf3,vf1,vf2
			addi t3,-4
			sqc2 vf3,0(t0)
		bgtz t3,Loop
			addi t0,16
		Escape:
	"					: /* Outputs. */
						:  "r"(R.Values),"r" (X1.Values),"r"(X2.Values),"r"(R.size)
						:);
#endif
#else
	if(X1.size<=4)
	{
		__asm
		{
			mov esi,X1
			mov ebx,[esi+4]    
			movaps xmm0,[ebx]
			mov esi,X2
			mov ecx,[esi+4]
			subps xmm0,[ecx]
			mov esi,R
			mov eax,[esi+4]  
			movaps [eax],xmm0
		}
		return;
	}
	__asm
	{
		mov esi,X1
		mov ebx,[esi+4]
		mov esi,X2
		mov ecx,[esi+4]
		mov esi,R
		mov eax,[esi+4]      
		mov edx,[esi+8]
		Eq_Loop:
			movaps xmm0,[ebx]
			add ebx,16
			subps xmm0,[ecx]  
			add ecx,16
			movaps [eax],xmm0
			add eax,16
			sub edx,4
			cmp edx,0
		jg Eq_Loop
	}
#endif
}                     
//------------------------------------------------------------------------------                            
void Add(Vector &R,const Vector &X1,const Vector &X2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(R.size>X1.size||R.size>X2.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}           
	if(R.size==0)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<R.size;i++)
		R.Values[i]=X1.Values[i]+X2.Values[i];
#else
	asm __volatile__("
	.set noreorder  
		blez %2,Escape
		add t4,zero,zero
		add t1,zero,%0
		add t2,zero,%1
		add t3,zero,%3
		sub t8,%2,t4
		//nop
		Loop:
			lqc2 vf1,0(t1)
			lqc2 vf2,0(t2)
			vadd vf3,vf1,vf2
			sqc2 vf3,0(t3)
			addi t1,16
			addi t2,16
			addi t3,16
			addi t4,4
			sub t8,%2,t4
			bgtz t8,Loop
			nop
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "r" (X1.Values), "r" (X2.Values),"r"(R.size),"r"(R.Values)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	__asm
	{
		mov esi,X1
		mov ebx,[esi+4]
		mov esi,X2
		mov ecx,[esi+4]
		mov esi,R
		mov eax,[esi+4]
		mov edx,[esi+8]
		movaps xmm0,[ebx]
		Eq_Loop:
			movaps xmm0,[ebx]
			addps xmm0,[ecx]
			movaps [eax],xmm0
			add eax,16
			add ebx,16
			sub edx,4
			cmp edx,0
		jg Eq_Loop
	}
#endif
}
//------------------------------------------------------------------------------
void Vector::operator+=(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size<v.size)
	{
		unsigned errorhere=1;
		errorhere=2;
		errorhere;
		throw Vector::BadSize();
	}
	if(size==0)
		throw Vector::BadSize();
#endif         
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<v.size;++i)
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
		mov esi,v
		mov ebx,[esi+4]   // ebx=v.Values    
		mov ecx,[esi+8]
		mov esi,this
		mov eax,[esi+4]   // eax=Values
		movaps xmm0,[ebx]
		Eq_Loop:
			movaps xmm0,[ebx]
			addps xmm0,[eax]
			movaps [eax],xmm0
			add eax,16
			add ebx,16
			sub ecx,4
			cmp ecx,0
		jg Eq_Loop
	}
#endif
}
//------------------------------------------------------------------------------
void Vector::AddLarger(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size>v.size)
	{
		unsigned errorhere=1;
		errorhere=2;
		errorhere;
		throw Vector::BadSize();
	}
#endif
	unsigned i;
	for(i=0;i<size;++i)
		Values[i]+=v.Values[i];
}
//------------------------------------------------------------------------------
Vector& Vector::operator-=(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size<v.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}          
#endif
#ifndef SIMD        
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<v.size;++i)
		Values[i]-=v.Values[i];
#else
	asm __volatile__("  
		.set noreorder
		blez %2,Escape
		nop
		ble %2,3,Tail
		Loop:
			addi %2,-4
			lqc2 vf1,0(%0)
			lqc2 vf2,0(%1)
			vsub vf3,vf1,vf2
			sqc2 vf3,0(%0)
			addi %0,16
		bgt %2,3,Loop
			addi %1 ,16
		Tail:
			lwc1 f1,0(%0)
			lwc1 f2,0(%1)
			sub.s f3,f1,f2
			swc1 f3,0(%0)
		beq %2,1,Escape
			lwc1 f1,4(%0)
			lwc1 f2,4(%1)
			sub.s f3,f1,f2
			swc1 f3,4(%0)
		beq %2,2,Escape  
			lwc1 f1,8(%0)
			lwc1 f2,8(%1)
			sub.s f3,f1,f2
			swc1 f3,8(%0)
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "g" (Values), "g" (v.Values),"g"(v.size)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	__asm
	{
		mov esi,v
		mov ebx,[esi+4]   // ebx=v.Values  
		mov ecx,[esi+8]   // v.size
		mov esi,this
		mov eax,[esi+4]   // eax=Values
		cmp ecx,4
		jl Tail
		Eq_Loop:
			movaps xmm0,[eax]
			subps xmm0,[ebx]
			movaps [eax],xmm0
			add eax,16
			add ebx,16
			sub ecx,4
			cmp ecx,3
		jg Eq_Loop
		Tail:
		cmp ecx,0
		jle Escape
			movss xmm0,[eax]
			subss xmm0,[ebx]
			movss [eax],xmm0
		cmp ecx,1
		je Escape
			add eax,4
			add ebx,4
			movss xmm0,[eax]
			subss xmm0,[ebx]
			movss [eax],xmm0    
		cmp ecx,2
		je Escape
			add eax,4
			add ebx,4
			movss xmm0,[eax]
			subss xmm0,[ebx]
			movss [eax],xmm0
		Escape:
	}
#endif          
	return *this;
}                     
// _MM_SHUFFLE(z,y,x,w) ((z<<6)|(y<<4)|(x<<2)|w)

float operator*(const Vector &v1,const Vector &v2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(v1.size>v2.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}        
	if(v1.size==0||v2.size==0)   
		return 0;
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	float total=0.0f;
	if(v1.size>v2.size)
		return 0;
	for(unsigned i=0;i<v1.size;i++)
		total+=v1.Values[i]*v2.Values[i];
	return total;
#else
	if(v1.size<4)
	{
		if(v1.size<3)
		{
			if(v1.size<2)
			{
				if(v1.size==1)
					return v1.Values[0]*v2.Values[0];
				else
					return 0;
			}
			else
			{
				return v1.Values[0]*v2.Values[0]+v1.Values[1]*v2.Values[1];
			}
		}
		else
		{
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
		}
	}
	else
	{
	asm __volatile__("
		.set noreorder   
		add t0,zero,%3
		blez t0,Escape   
		add t1,zero,%1			// Values[0]
		add t2,zero,%2			// Values[0]
		sub.s f3,f0,f0
		ble t0,3,Loop2
		vsuba ACC,vf0,vf0
		Loop:
			lqc2 vf1,0(t1)		// Load 4 numbers into vf1
			lqc2 vf2,0(t2)		// Load 4 numbers into vf2
			vmadda ACC,vf1,vf2
			addi t1,16			// next four numbers
			addi t0,-4			// counter-=4
		bgt t0,3,Loop
			addi t2,16
// Now ACC contains 4 sums to be added together.
		vmaddx vf4,vf0,vf0x
		vaddw vf5,vf4,vf4w
		vaddz vf5,vf4,vf5z
		vaddy vf4,vf4,vf5y  
		sqc2 vf4,0(%0)
		blez t0,Escape
		lwc1 f3,0(%0)
		Loop2:
			lwc1 f1,0(t1)		// Load 4 numbers into vf1
			lwc1 f2,0(t2)		// Load 4 numbers into vf2
			mul.s f4,f1,f2
			add.s f3,f3,f4
			addi t1,4			// next four numbers
			addi t0,-1			// counter-=4
		bgtz t0,Loop2
			addi t2,4
		swc1 f3,0(%0)
		Escape:
		.set reorder   
	"					:  
						:  "r" (TempVector3.Values),"r" (v1.Values), "r" (v2.Values),"g"(v1.size)
						:  );
		return TempVector3.Values[0];
	}
#endif
#else
/*	if(v1.size==3)
	{
		float temp;
		__asm
		{
		// Load v1's float address into eax
			mov eax,v1//t1
			mov eax,[eax+4]
		// Load from eax early into xmm0
			movss xmm0,[eax]
		// Load v2's float address into ebx
			mov ebx,v2//t2   
		// Multiply the first two values together
			mulss xmm0,[ebx]
			mov ebx,[ebx+4]
			movss xmm1,[eax+4]
			movss xmm2,[eax+8]
			mulss xmm1,[ebx+4]
			mulss xmm2,[ebx+8]
		// Multiply with v2's data, and store the result in xmm0
			addss xmm0,xmm1
			addss xmm0,xmm2
		// Put the contents of xmm0[0] back into v1.
			movss temp,xmm0*
			mov eax,v1
			mov eax,[eax+4]
			movaps xmm0,[eax]
			mov ebx,v2 
			mov ebx,[ebx+4]
			movaps xmm1,[ebx]
			//mov ecx,t3
			mulps xmm0,xmm1
			movaps xmm2,xmm0
			shufps xmm0,xmm0,9
			addps xmm0,xmm2
			shufps xmm2, xmm2,18
			addss xmm0,xmm2
		// Put the contents of xmm0[0] back into v1.
			movss temp,xmm0
		}
		return temp;
	}
	if(v1.size==4)
	{
		float temp;
		float *t3=&temp;
		__asm
		{
		// Load v1's float address into eax
			mov eax,v1//t1
			mov eax,[eax+4]
		// Load v2's float address into ebx
			mov ebx,v2 
			mov ebx,[ebx+4]
		// Load the return float address into ecx
			mov ecx,t3
		// Load v1's data into xmm0
			movaps xmm0,[eax]
			movaps xmm1,[ebx]
		// Multiply with v2's data, and store the result in xmm0
			mulps xmm0,xmm1
			movaps xmm2,xmm0
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
			shufps xmm0,xmm0,9
		// Add them together
			addps xmm0,xmm2
			shufps xmm2, xmm2,18
			addss xmm0,xmm2
		// Put the contents of xmm0[0] back into v1.
			movss [ecx],xmm0
		}
		return temp;
	}*/
	float temp;
	_asm
	{         
		xorps xmm2,xmm2
		mov esi,v2
		mov ebx,[esi+4]
		mov eax,ebx
		and eax,15

		mov esi,v1  
		mov ecx,[esi+8]  

		cmp eax,0
		jne Unaligned

		mov eax,[esi+4]
		and eax,15

		cmp eax,0
		jne Unaligned

		mov eax,[esi+4]

		cmp ecx,4
		jl Tail
		Dot_Product_Loop:
			movaps xmm1,[eax]
			add eax,16
			add ecx,-4
			mulps xmm1,[ebx]
			add ebx,16
			cmp ecx,3
			addps xmm2,xmm1
		jg Dot_Product_Loop
		Tail:
		cmp ecx,0
		je AddUp
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm2,xmm1
		cmp ecx,1
		je AddUp
			movss xmm1,[eax+4]
			mulss xmm1,[ebx+4]
			addss xmm2,xmm1
		cmp ecx,2
		je AddUp
			movss xmm1,[eax+8]
			mulss xmm1,[ebx+8]
			addss xmm2,xmm1
		AddUp:
		// Now add the four sums together:
		movhlps xmm1,xmm2
		addps xmm2,xmm1         // = X+Z,Y+W,--,--
		movaps xmm1,xmm2
		shufps xmm1,xmm1,1      // Y+W,--,--,--
		addss xmm2,xmm1
		jmp Escape
	Unaligned:     
		mov esi,v1
		mov eax,[esi+4]
		mov esi,v2
		mov ebx,[esi+4]
		UnalignedLoop:
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm2,xmm1
			add eax,4
			add ebx,4
			add ecx,-1
			cmp ecx,0
		jg UnalignedLoop
		Escape:
		movss temp,xmm2
	}
	return temp;
#endif
}
bool DotProductTestGE(const Vector &v1,const Vector &v2,float f)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(v1.size!=3||v2.size!=3)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}
#endif
#if 1 //ndef SIMD
	return (v1*v2>=f);
#else
	float temp;
	__asm
	{
		movss xmm4,f
		shufps xmm4,xmm4,0		// put f into each value of xmm4
	// Load v1's float address into eax
		mov eax,v1
		mov eax,[eax+4]
	// Load v1's data into xmm0
		movaps xmm0,[eax]
	// Load v2's float address into ebx
		mov ebx,v2 
		mov ebx,[ebx+4]
	// Multiply v1 with v2's data, and store the result in xmm0
		mulps xmm0,[ebx]
	// is xmm0.0 greater than f?
		cmplts xmm4,xmm0

	// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movhlps xmm2,xmm0	
	// Add them together, we're only interested in the lower results
		addps xmm0,xmm2
	// Shuffle xmm0's second value into xmm2's first:
		pshufd	xmm2,xmm0,01010101b	
		addss xmm0,xmm2
	// Put the contents of xmm0[0] back into v1.
		movss temp,xmm0
	}
	return (temp>f);
#endif
}
//------------------------------------------------------------------------------
void MultiplyElements(Vector &Ret,const Vector &V1,const Vector &V2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(V1.size>V2.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}
	if(V1.size==0||V2.size==0)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<V1.size;i++)
		Ret.Values[i]=V1.Values[i]*V2.Values[i];
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
	_asm
	{
		mov esi,V2
		mov ebx,[esi+4]

		mov esi,V1
		mov eax,[esi+4]
		mov esi,[esi+8]
                      
		mov edi,Ret
		mov ecx,[edi+4]

		xorps xmm2,xmm2
		Dot_Product_Loop:
			movaps xmm1,[eax]
			movaps xmm2,[ebx]
			mulps xmm1,[ebx]
			movaps [ecx],xmm1
			add eax,16
			add ebx,16  
			add ecx,16
			add esi,-4
			cmp esi,0
		jg Dot_Product_Loop
	}
#endif
}              
void MultiplyElements8(Vector &Ret,const Vector &V1,const Vector &V2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(V1.size>V2.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}
	if(V1.size==0||V2.size==0)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<8;i++)
		Ret.Values[i]=V1.Values[i]*V2.Values[i];
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
	_asm
	{
		mov esi,V1
		mov eax,[esi+4]
		movaps xmm1,[eax]
		movaps xmm3,[eax+16]
		mov esi,V2
		mov ebx,[esi+4]
		movaps xmm2,[ebx]
		movaps xmm4,[ebx+16]
		mov edi,Ret
		mov ecx,[edi+4]

		mulps xmm1,xmm2
		mulps xmm3,xmm4
		movaps [ecx],xmm1
		movaps [ecx+16],xmm3
	}
#endif
}
//------------------------------------------------------------------------------
void MultiplyElementsAndAdd(Vector &V3,const Vector &V1,const Vector &V2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(V1.size>V2.size)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Vector::BadSize();
	}
	if(V1.size==0||V2.size==0)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<V1.size;i++)
		V3.Values[i]+=V1.Values[i]*V2.Values[i];
#else
	V3;
	V1;
	V2;
	asm __volatile__("  
		.set noreorder
		Loop:
			lqc2 vf01,0(%1)
			lqc2 vf02,0(%2)
			vmul vf03,vf01,vf02  
			lqc2 vf4,0(%0)
			vadd vf4,vf4,vf3
			sqc2 vf4,0(%0)
			addi %0,16
			addi %1,16
			addi %3,-4
		bgtz %3,Loop
			addi %2,16
	"					: /* Outputs. */ 
						: "r"(V3.Values),"r"(V1.Values),"r"(V2.Values),"r"(V1.size)
						: /* Clobber */);
#endif
#else
//	for(unsigned i=0;i<V1.size;i++)
//		ret.Values[i]+=V1.Values[i]*V2.Values[i];
	_asm
	{
		mov esi,V2
		mov ebx,[esi+4]

		mov esi,V1
		mov eax,[esi+4]
		mov esi,[esi+8]
                      
		mov edi,V3
		mov ecx,[edi+4]

		xorps xmm2,xmm2
		Dot_Product_Loop:
			movaps xmm1,[eax]
			add eax,16
			mulps xmm1,[ebx]  
			add ebx,16  
			addps xmm1,[ecx]
			add esi,-4
			movaps [ecx],xmm1
			add ecx,16
			cmp esi,0
		jg Dot_Product_Loop
	}
#endif
}
//------------------------------------------------------------------------------
float DotDifference(const Vector &D,const Vector &X1,const Vector &X2)
{                          
#ifdef CHECK_MATRIX_BOUNDS
	if(X1.size!=3)
		throw Vector::BadSize();
	if(X2.size!=3)
		throw Vector::BadSize();
	if(D.size!=3)
		throw Vector::BadSize();
#endif    
	float temp;
#ifndef SIMD
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
		mov eax,[eax+4]
	// Load X2's float address into ebx
		mov ebx,X2
		mov ebx,[ebx+4]       
	// Load D's float address into ebx
		mov ecx,D
		mov ecx,[ecx+4]
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
float DotSum(const Vector &D,const Vector &X1,const Vector &X2)
{                          
#ifdef CHECK_MATRIX_BOUNDS
	if(X1.size!=3)
		throw Vector::BadSize();
	if(X2.size!=3)
		throw Vector::BadSize();
	if(D.size!=3)
		throw Vector::BadSize();
#endif      
	float temp;
#ifndef SIMD
#ifndef PLAYSTATION2
	temp=D(0)*(X1(0)+X2(0))+D(1)*(X1(1)+X2(1))+D(2)*(X1(2)+X2(2));
#else
		static float d[4];
		asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v1's float address into vf1
			lqc2 vf03,0(%3)		// Load v1's float address into vf1
		// Multiply them together, and store the result in vf01
			vadd vf1,vf1,vf2
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
	float *t3=&temp;
	__asm
	{
	// Load X1's float address into eax
		mov eax,X1
		mov eax,[eax+4]
	// Load X2's float address into ebx
		mov ebx,X2
		mov ebx,[ebx+4]       
	// Load D's float address into ebx
		mov ecx,D
		mov ecx,[ecx+4]
	// Load the return float address into esi
		mov esi,t3
	// Load v1's data into xmm0
		movaps xmm0,[eax]     
		addps xmm0,[ebx]
	// Multiply them together, and store the result in xmm0
		mulps xmm0,[ecx]
	// Shuffle xmm0's two upper Values into xmm2's two lower slots
		shufps xmm2,xmm0,0xBB
		shufps xmm2,xmm2,0xBB
	// Add them together, we're only interested in the lower results
		addps xmm0,xmm2
	// Shuffle xmm0's two upper Values into xmm2's two lower slots
		shufps xmm2,xmm0,0x55
		shufps xmm2,xmm2,0xBB
		addps xmm0,xmm2
	// Put the contents of xmm0[0] into t3.
		movss [esi],xmm0
	}
#endif           
	return temp;
}
//------------------------------------------------------------------------------
void Vector::operator*=(float f)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<size;++i)
	{
		Values[i]*=f;
	}
#else			// line_d*a
	f;
	asm __volatile__("
		.set noreorder  
		lwc1 f2,0(%2)
		mfc1 t8,f2			// t8=f2
		ctc2 t8,$vi21		// put in the immediate I
		nop
		nop
		nop
		add t0,zero,%1			// size
		blez t0,Escape			// for zero width matrix 
		add t1,zero,%0			// t1= address of matrix 1's row
		iLoop:
			lqc2 vf1,0(t1)		// Load 4 numbers into vf1
		vmuli vf2,vf1,I		
			sqc2 vf2,0(t1)		// next four numbers
			addi t0,-4			// counter-=4
		bgtz t0,iLoop
			addi t1,16
		Escape:
	"					: 
						: "r" (Values),"r"(size) ,"r"(&f)
						: );
#endif
#else
	if(size==0)
		throw Vector::BadSize();
	if(size<4)
	{
		if(size<3)
		{
			if(size==1)   
				Values[0]*=f;
			else
			{
				Values[0]*=f;
				Values[1]*=f;
			}
		}
		else
		{
			Values[0]*=f;
			Values[1]*=f;
			Values[2]*=f;
		}
	}
	else
	{           
		__asm
		{
			//mov esi,&f
			movss xmm1,f//[ebp+12]
			shufps xmm1,xmm1,0
			mov esi,this
			mov eax,[esi+4]
			mov ecx,[esi+8]
			cmp ecx,0
			jle Escape
			iLoop:
				movaps xmm0,[eax]
				mulps xmm0,xmm1
				movaps [eax],xmm0
				add eax,16
				add ebx,16
				add ecx,-4
				cmp ecx,0
			jg iLoop
			Escape:
		};
	}
/*	__asm
	{
		movss xmm0,[ebp+12]
		shufps xmm0,xmm0,0x0
		mov edi,this		// matrix's Values
		mov ebx,[edi+4]
		mulps xmm0,[ebx]
		movaps [ebx],xmm0
	}         */
#endif
}
void Vector::operator/=(float f)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<size;++i)
		Values[i]/=f;
#else
	//static float d[4];
	//d[0]=f;
	asm __volatile__("
		.set noreorder
//		CHECK THIS
		add t0,zero,%2				// V1.size
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
						: "r"(Values),"r"(&f),"r"(size)
						: );
#endif
#else
	if(size>4)
	{       
		unsigned i;
		for(i=0;i<size;i++)
			Values[i]/=f;
		return;
	}
	__asm
	{     
		movss xmm0,f	//esp+4
		shufps xmm0,xmm0,0x0
		mov edi,this		// matrix's Values
		mov ebx,[edi+4]
		movaps xmm1,[ebx]
		divps xmm1,xmm0
		movaps [ebx],xmm1
	}
#endif
}                   
//------------------------------------------------------------------------------
Vector operator*(float d,const Vector &v2)
{
	Vector v(v2.size);
	for(unsigned i=0;i<v.size;i++)
		v(i)=d*v2.Values[i];
	return v;
}
Vector operator*(const Vector &v1,float d)
{
	unsigned i;
	Vector v(v1.size);
	for(i=0;i<v.size;i++)
	{
		v(i)=d*v1.Values[i];
	}
	return v;
}
Vector operator / (const Vector &v1,float d)
{
	unsigned i;
	Vector v(v1.size);
	for(i=0;i<v.size;i++)
	{
		v.Values[i]=v1.Values[i]/d;
	}
	return v;
}
//------------------------------------------------------------------------------
Vector Vector::SubVector(unsigned start,unsigned length) const
{
	Vector v(length);
#ifdef CHECK_MATRIX_BOUNDS
	if(start+length>size)
		throw Vector::BadSize();
	if(start<0)
		throw Vector::BadSize();
	if(length<=0)
		throw Vector::BadSize();
#endif
	unsigned i;
	for(i=0;i<length;++i)
	{
		v.Values[i]=Values[start+i];
	}
	return v;
}
//------------------------------------------------------------------------------
Vector operator^(const Vector &v1,const Vector &v2)
{
	Vector v(3);
	v.Values[0]=v1.Values[1]*v2.Values[2]-v2.Values[1]*v1.Values[2];
	v.Values[1]=v1.Values[2]*v2.Values[0]-v2.Values[2]*v1.Values[0];
	v.Values[2]=v1.Values[0]*v2.Values[1]-v2.Values[0]*v1.Values[1];
	return v;
}
//------------------------------------------------------------------------------
void CrossProduct(Vector &result,const Vector &v1,const Vector &v2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
#ifdef SIMUL_EXCEPTIONS
	if(result.size==0||v1.size==0||v2.size==0)
		throw Vector::BadSize();
#endif
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
		mov esi,v1			// Vector's Values
		mov eax,[esi+4]		// Put Vector's Values in ebx
		mov esi,v2			// Vector's Values
		mov ebx,[esi+4]		// Put Vector's Values in ebx
		mov esi,result		// Vector's Values
		mov ecx,[esi+4]		// Put Vector's Values in ebx
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
// Size
//------------------------------------------------------------------------------ 
bool Vector::operator>(float R) const
{
	float *V=Values;
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
	for(unsigned i=0;i<size;i++)
		total+=Values[i]*Values[i];
	total=sqrt(total);
	return (total>R);
#else
		static float total;
		static float *t3=&total;
		__asm
		{
			mov eax,this
			mov eax,[eax+4]
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
float Vector::SquareSize(void) const
{
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i;
	float total=0.0f;
	for(i=0;i<size;i++)
		total+=Values[i]*Values[i];
#ifdef SIMUL_EXCEPTIONS
	if(total<0||total>1e24)
		throw BadNumber();
#endif
	return total;
#else         
	if(size<=0)
		return 0;
	if(size<=4)
	{
		static float temp;
		static float *t3=&temp;
		__asm
		{
			mov eax,this
			mov eax,[eax+4]
			mov ecx,t3
			movaps xmm0,[eax]
			mulps xmm0,xmm0
			shufps xmm2,xmm0,0xBB
			shufps xmm2,xmm2,0xBB
			addps xmm0,xmm2
			shufps xmm2,xmm0,0x55
			shufps xmm2,xmm2,0xBB
			addps xmm0,xmm2
			movss [ecx],xmm0
		}
		return temp;
	}
	else
	{         

		float total=0.0f;
		for(unsigned i=0;i<size;i++)
			total+=Values[i]*Values[i];
		if(total<0||total>1e24)
 			throw BadNumber();
		return total;
	}
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
void UnitRateVector(Vector &Rate,Vector &Sized,Vector &SizedRate)
{
	float sz=Sized.Magnitude();
	Rate=SizedRate/sz-Sized*(Sized*SizedRate)/(sz*sz*sz);
}
//------------------------------------------------------------------------------
void Swap(Vector &V1,Vector &V2)
{
	std::swap(V1.size,V2.size);
	std::swap(V1.Values,V2.Values);
#ifdef CANNOT_ALIGN
	std::swap(V1.V,V2.V);
#endif
}
//------------------------------------------------------------------------------
void AddFloatTimesLargerVector(Vector &V2,const float f,const Vector &V1)
{
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<V2.size;i++)
	{
		V2.Values[i]+=V1.Values[i]*f;
	}
#else        
	static float d[4];
	d[0]=f;
	unsigned W16=V2.S16/4;
	asm __volatile__("
		.set noreorder  
		lqc2 vf4,0(%3)
		vsub vf3,vf00,vf00
		vaddx vf3,vf3,vf4x				// All 4 numbers are equal to multiplier
		add t0,zero,%0					// t0= number of 4-wide columns
		add t1,zero,%1					// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape					// for zero width matrix 
		nop
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			lqc2 vf2,0(t2)			// Load 4 numbers into vf2
			vmul vf4,vf1,vf3
			vadd vf2,vf2,vf4
			sqc2 vf2,0(t2)
			addi t1,16				// next four numbers
			addi t2,16
			addi t0,-1				// counter-=4
			bgtz t0,iLoop
			nop
		nop		
		Escape:
	"					: 
						: "r" (W16), "r" (V1.Values), "r"(V2.Values),"r" (d)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}                                   
//------------------------------------------------------------------------------  
#ifndef PLAYSTATION2
#ifndef SIMD
void AddFloatTimesVector(Vector &V2,const float f,const Vector &V1)
{
	for(unsigned i=0;i<V1.size;i++)
		V2.Values[i]+=V1.Values[i]*f;
}
#else
void AddFloatTimesVector(Vector &V2,const float f,const Vector &V1)
{
	__asm
	{
		movss xmm1,f		//[ebp+12]   //was [edi]
		shufps xmm1,xmm1,0
		mov esi,V1
		mov ebx,[esi+4]		// ebx=v.Values
		add esi,8			// v.size
		mov edx,[esi]
		mov esi,V2
		mov eax,[esi+4]		// eax=Values
		movaps xmm0,[ebx]

		mov ecx,ebx
		and ecx,15
		cmp ecx,0
		jne Unaligned     
		mov ecx,eax
		and ecx,15
		cmp ecx,0
		jne Unaligned

		cmp edx,4
		jl Tail
		Eq_Loop:
			movaps xmm0,[ebx]
			add ebx,16
			mulps xmm0,xmm1
			sub edx,4
			addps xmm0,[eax]
			add eax,16
			movaps [eax-16],xmm0
			cmp edx,0
		jg Eq_Loop
		jmp Escape
		Tail:
		cmp edx,0
		je Escape
			movss xmm0,[ebx]
			mulss xmm0,xmm1
			addss xmm0,[eax]
			movss [eax],xmm0
		cmp edx,1
		je Escape
			movss xmm0,[ebx+4]
			mulss xmm0,xmm1
			addss xmm0,[eax+4]
			movss [eax+4],xmm0
		cmp edx,2
		je Escape
			movss xmm0,[ebx+8]
			mulss xmm0,xmm1
			addss xmm0,[eax+8]
			movss [eax+8],xmm0
		jmp Escape
		Unaligned:   
		U_Loop:
			movss xmm0,[ebx]
			mulss xmm0,xmm1
			addss xmm0,[eax]
			movss [eax],xmm0
			add eax,4
			add ebx,4
			add edx,-1
			cmp edx,0
		jg U_Loop

		Escape:
	}
}
#endif
#else    
void AddFloatTimesVector(Vector &V2,const float f,const Vector &V1)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(V2.size!=V1.size)
		throw Vector::BadSize();
	if(V2.size==0)
		throw Vector::BadSize();
#endif
	static float d[4];
	d[0]=f;
	asm __volatile__("
		.set noreorder  
		lqc2 vf4,0(%3)
		vsub vf3,vf00,vf00
		vaddx vf3,vf3,vf4x		// All 4 numbers are equal to multiplier
		add t0,zero,%0			// t0= number of 4-wide columns
		add t1,zero,%1			// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape			// for zero width matrix 
		and t3,%1,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		nop
		bnez t3,uLoop			// if not, do the unaligned copy
		and t3,%2,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		nop
		bnez t3,uLoop			// if not, do the unaligned copy
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
}
#endif
void SubtractFloatTimesVector(Vector &V2,const float f,const Vector &V1)
{
	unsigned i;
	for(i=0;i<V1.size;i++)
	{
		V2.Values[i]-=V1.Values[i]*f;
	}
}
void Multiply(Vector &V2,const float f,const Vector &V1)
{      
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<V1.size;i++)
	{
		V2.Values[i]=V1.Values[i]*f;
	}
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
	if(V1.size>4)
	{
		for(unsigned i=0;i<V1.size;i++)
			V2.Values[i]=V1.Values[i]*f;
	}
	__asm
	{
		//mov esi,[ebp+12]		//esp+4
		movss xmm0,f//[ebp+12]
		shufps xmm0,xmm0,0x0
		mov edi,V1
		mov esi,V2
		mov ebx,[edi+4]  
		mov eax,[esi+4]
		mulps xmm0,[ebx]
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void AddCrossProduct(Vector &result,const Vector &V1,const Vector &V2)
{
#ifdef SIMD
	if(result.size==0||V1.size==0||V2.size==0)
		throw Vector::BadSize();
	_asm
	{
		mov esi,V1			// Vector's Values
		mov eax,[esi+4]		// Put Vector's Values in ebx
		mov esi,V2			// Vector's Values
		mov ebx,[esi+4]		// Put Vector's Values in ebx   
		mov esi,result		// Vector's Values
		mov ecx,[esi+4]		// Put Vector's Values in ecx     
//							mov edx,t
		movaps xmm1,[eax]
		movaps xmm4,xmm1
		movaps xmm2,[ebx]
		movaps xmm3,xmm2
		shufps xmm1,xmm1,0x9	// << 1,2,0 -> 0x9
//							movups [edx],xmm1
		shufps xmm2,xmm2,0x12	// << 2,0,1 -> 0x12
//							movups [edx],xmm2
		shufps xmm3,xmm3,0x9	// << 1,2,0 -> 0x9
//							movups [edx],xmm3
		shufps xmm4,xmm4,0x12	// << 2,0,1 -> 0x12
//							movups [edx],xmm4
		mulps xmm1,xmm2
		mulps xmm3,xmm4
		subps xmm1,xmm3  
		movaps xmm0,[ecx]
		addps xmm0,xmm1
		movaps [ecx],xmm0
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
void AddDoubleCross(Vector &result,const Vector &V1,const Vector &V2)
{
	float a0,a1,a2;
	a0=V1.Values[1]*V2.Values[2]-V2.Values[1]*V1.Values[2];
	a1=V1.Values[2]*V2.Values[0]-V2.Values[2]*V1.Values[0];
	a2=V1.Values[0]*V2.Values[1]-V2.Values[0]*V1.Values[1];
	result.Values[0]+=V1.Values[1]*a2-a1*V1.Values[2];
	result.Values[1]+=V1.Values[2]*a0-a2*V1.Values[0];
	result.Values[2]+=V1.Values[0]*a1-a0*V1.Values[1];
}
//------------------------------------------------------------------------------
void SubtractDoubleCross(Vector &result,const Vector &V1,const Vector &V2)
{
	float a0,a1,a2;
	a0=V1.Values[1]*V2.Values[2]-V2.Values[1]*V1.Values[2];
	a1=V1.Values[2]*V2.Values[0]-V2.Values[2]*V1.Values[0];
	a2=V1.Values[0]*V2.Values[1]-V2.Values[0]*V1.Values[1];
	result.Values[0]-=V1.Values[1]*a2-a1*V1.Values[2];
	result.Values[1]-=V1.Values[2]*a0-a2*V1.Values[0];
	result.Values[2]-=V1.Values[0]*a1-a0*V1.Values[1];
}
//------------------------------------------------------------------------------
void ShiftElement(Vector &V1,Vector &V2,unsigned Idx2)
{
	if(V1.size>=((V1.size+3)&~0x3))
	{              
		if(!V1.size)
		{
			V1.Resize(1);
			V1.size=0;
		}
		else
		{
			static Vector temp;
			temp=V1;
			V1.Resize(V1.size+4);
			V1=temp;
		}
	}
	V1.Values[V1.size]=V2.Values[Idx2];
	for(unsigned i=Idx2;i<V2.size;i++)
		V2.Values[i]=V2.Values[i+1];
	V2.Values[V2.size]=0;
	V1.size++;
	V2.size--;
}
//------------------------------------------------------------------------------
void Vector::Cut(unsigned Idx)
{
	for(unsigned i=Idx;i<size-1;i++)
		Values[i]=Values[i+1];
	Values[size]=0;       
	size--;
}             
//------------------------------------------------------------------------------
void Vector::CutUnordered(unsigned Idx)
{                  
	size--;
	Values[Idx]=Values[size];
	Values[size]=0;
}
//------------------------------------------------------------------------------
void Vector::Insert(unsigned Idx)
{
	if(size>=((size+3)&~0x3))
	{
		if(!size)
		{
			Resize(1);   
			Values[0]=0;
			return;
		}
		else
		{
			static Vector temp;
			temp=*this;
			Resize(size+4);
			*this=temp;
		}
	}
	for(unsigned i=Idx;i<size;i++)
	{
		Values[i+1]=Values[i];
	}
	Values[Idx]=0;
	size++;
}        
//------------------------------------------------------------------------------
void Vector::Append(float s)
{
	if(size>=((size+3)&~0x3))
	{
		if(!size)
		{
			Resize(1); 
			Values[0]=s;
			return;
		}
		else
		{
			static Vector temp;
			temp=*this;
			Resize(size+4);
			*this=temp;
		}
	}
	Values[size]=s;
	size++;
}

#include "Vector3.h"
//------------------------------------------------------------------------------
void Vector::Insert(const Vector3 &v,unsigned pos)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(pos+3>size)
		throw Vector::BadSize();
#endif
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<3;i++)
		Values[pos+i]=v.Values[i];
#else                
	float *Row1=&(Values[pos]);
	_asm
	{             
		mov esi,v
		mov edi,this
		mov eax,3			// v.size
		mov ebx,Row1
		mov ecx,v
		mov edx,ebx
		and edx,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		cmp edx,0
		jne uLoop			// if not, do the unaligned copy
		mov esi,[edi+8]		// size
		mov edx,pos
		add edx,eax			// pos+v.size
		cmp eax,4
		jl Unaligned
		Loop4:
			movaps xmm1,[ecx]	// Load 4 numbers from ecx
			movaps [ebx],xmm1	// Move 4 numbers into ebx
			add ebx,16			// next four numbers
			add ecx,16              
			add eax,-4			// counter-=4
			cmp eax,3
		jg Loop4
		Unaligned:
		cmp eax,0
		jle Escape				// Exactly 4 or overrun
		uLoop:
			mov edx,[ecx]		// Load 1 number into vf1
			mov [ebx],edx		// Move 1 number into vf2
			add ebx,4			// next four numbers
			add ecx,4
			add eax,-1			// counter-=4
		jg uLoop
		Escape:
	};
#endif
#else
	float *Row1=&(Values[pos]);
	asm __volatile__("
		.set noreorder
		add t1,zero,%1
		add t2,zero,%2
		add t0,zero,%0			// v.size
		blez t0,Escape
		and t3,t1,15			// And with 0xF to see if it's aligned - if pos is a multiple of 4
		bnez t3,uLoop			// if not, do the unaligned copy
		add t5,zero,%3			// size
		//add t7,t0,%4			// pos+v.size
		//beq t7,t5,NoTail
		//nop
		NoTail:
		blt t0,4,Unaligned
		nop
		Loop:
			addi t0,-4			// counter-=4
			lqc2 vf2,0(t2)		// Load 4 numbers into vf1
			sqc2 vf2,0(t1)		// Load 4 numbers into vf2
			addi t1,16			// next four numbers
		bgt t0,3,Loop
			addi t2,16
		blez t0,Escape			// Exactly 4 or overrun
		Unaligned:
		uLoop:
			lwc1 f2,0(t2)		// Load 4 numbers into vf1
			swc1 f2,0(t1)		// Load 4 numbers into vf2
			addi t1,4			// next four numbers
			addi t0,-1			// counter-=4
		bgtz t0,uLoop
			addi t2,4
		Escape:
		.set reorder
	"					:
						: "r" (v.size), "r" (Row1), "r" (v.Values),"r"(size),"r"(pos)
						:  "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}

//------------------------------------------------------------------------------
void Vector::operator+=(const Vector3 &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size<3)
	{
		unsigned errorhere=1;
		errorhere=2;
		errorhere;
		throw Vector::BadSize();
	}
	if(size==0)
		throw Vector::BadSize();
#endif         
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]+=v.Values[i];
#else
	asm __volatile__("  
		.set noreorder
		Loop:
			lqc2 vf2,0(%1)
			addi %1,16
			lqc2 vf1,0(%0)
			vadd vf3,vf1,vf2
			sqc2 vf3,0(%0)
			addi %2,-4
		bgtz 3,Loop
			addi %0,16
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "g" (Values), "g" (v.Values)
						: );
#endif
#else
	__asm
	{
		mov ebx,v
		movaps xmm0,[ebx]
		mov esi,this
		mov eax,[esi+4]   // eax=Values
		addps xmm0,[eax]
		movaps [eax],xmm0
	}
#endif
}
//------------------------------------------------------------------------------
void Vector::operator-=(const Vector3 &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size<3)
	{
		unsigned errorhere=1;
		errorhere=2;
		errorhere;
		throw Vector::BadSize();
	}
	if(size==0)
		throw Vector::BadSize();
#endif         
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;++i)
		Values[i]-=v.Values[i];
#else
	asm __volatile__("  
		.set noreorder
		Loop:
			lqc2 vf2,0(%1)
			addi %1,16
			lqc2 vf1,0(%0)
			vsub vf3,vf1,vf2
			sqc2 vf3,0(%0)
			addi %2,-4
		bgtz 3,Loop
			addi %0,16
		Escape:
	"					: /* Outputs. */
						: /* Inputs */ "g" (Values), "g" (v.Values)
						: );
#endif
#else
	__asm
	{
		mov esi,this
		mov eax,[esi+4]   // eax=Values
		movaps xmm0,[eax]
		mov ebx,v
		subps xmm0,[ebx]
		movaps [eax],xmm0
	}
#endif
}            
//------------------------------------------------------------------------------
void Vector::InsertAdd(const Vector3 &v,unsigned pos)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(pos+3>size)
		throw Vector::BadSize();
#endif
	unsigned i;
	for(i=0;i<3;++i)
	{
		Values[pos+i]+=v[i];
	}
}            
//------------------------------------------------------------------------------
void Vector::SubVector(Vector3 &v,unsigned start) const
{
	for(unsigned i=0;i<3;++i)
		v.Values[i]=Values[start+i];
}
std::ostream &operator<<(std::ostream &out, Vector const &)
{
//	out.write((const char*)(&I.size),sizeof(I.size));
	//out.write((const char*)(I.FloatPointer(0)),sizeof(float)*I.size);
	return out;
}
std::istream &operator>>(std::istream &in, Vector &)
{
//	unsigned sz=0;
	//in.read((char*)(&sz),sizeof(sz));
	//I.Resize(sz);
	//in.read((char*)(I.FloatPointer(0)),sizeof(float)*I.size);
	return in;
}
}
}
