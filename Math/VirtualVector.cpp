#define SIM_MATH
#include "VirtualVector.h"
#include "Vector3.h"
#include <math.h>     
#include <algorithm>

#ifdef WIN32
	//#define SIMD
	#define CANNOT_ALIGN
#endif             
#undef PLAYSTATION2

namespace platform
{
	namespace math
	{    
VirtualVector::VirtualVector(void)
{
	Values=NULL;
	V=NULL;
	size=0;
}

VirtualVector::~VirtualVector(void)
{
	Values=NULL;
	V=NULL;
}

//------------------------------------------------------------------------------
void VirtualVector::operator=(const Vector &v)
{
	if(size!=v.size)
		Resize(v.size);
	Vector::operator|=(v);
}

void VirtualVector::operator=(const Vector3 &v)
{
	Values[0]=v(0);
	Values[1]=v(1);
	Values[2]=v(2);
}
void VirtualVector::Resize(unsigned sz)
{
	size=sz;
}
                  
void VirtualVector::PointTo(float *addr)
{
	Values=addr;
}                         
void VirtualVector::Zero(void)
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
			movss [esi],xmm0
			add esi,4
			add eax,-1
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
		add t1,zero,%1
		blez t1,iEscape
        xor t3,t3
        vsub vf1,vf0,vf0
        and t4,t0,15
		beqz t4,iLoop
        nop
		sw t3,0(t0)
		addi t3,4
		addi t1,-1
		beq t4,12,iLoop
		nop
		sw t3,4(t0)
		addi t3,4
		addi t1,-1
		beq t4,8,iLoop
		nop
		sw t3,8(t0)
		addi t3,4
		addi t1,-1
		iLoop:
			sw t3,0(t0)
			addi t1,-4
			addi t0,16
			nop
			nop
		bgt t1,3,iLoop
		iEscape:
        beqz t1,iLoop
		sw t3,0(t0)
        beq t1,1,iLoop
		sw t3,4(t0)
        beq t1,2,iLoop
		sw t3,8(t0)
		.set reorder
	"	:
		: "r" (Values),"r"(size)
		:  );
#else
	for(unsigned i=0;i<size;i++)
		Values[i]=0.f;
#endif
#endif
}
//------------------------------------------------------------------------------
// We have a separate implementation for *=
// because the first value is not necessarily 4-word aligned.
void VirtualVector::operator*=(float f)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<size;i++)
		Values[i]*=f;
#else
	f;
	asm __volatile__("
		.set noreorder  
		lwc1 f3,0(%2)				// f2 = multiplier f
		add t0,zero,%1				// V1.size
		blez t0,Escape				// for zero width matrix 
		add t1,zero,%0				// t1= address of  values
		iLoop:
			lwc1 f1,0(t1)			// Load 4 numbers into vf1
			mul.s f2,f1,f3		
			swc1 f2,0(t1)		// next four numbers
			addi t0,-1				// counter-=4
		bgtz t0,iLoop
			addi t1,4
		Escape:
	"					: 
						: "r" (Values),"r" (size),"r" (&f)
						: );
#endif
#else
	for(unsigned i=0;i<size;i++)
		Values[i]*=f;
#endif
}                
}
}

