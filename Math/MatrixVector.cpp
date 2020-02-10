#define SIM_MATH

#include <string.h>
#include <stdio.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include "DebugNew.h"			// Enable Visual Studio's memory debugging
#endif
#include <algorithm>

#include "Matrix.h"
#include "SimVector.h"  
#include "MatrixVector.h"

#ifdef WIN32
	//#define SIMD
#endif

#ifdef PLAYSTATION_2
	//#define PLAYSTATION2
#endif                 

#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#else
#undef CHECK_MATRIX_BOUNDS
#endif

namespace simul
{
	namespace math
	{
// Math
// 

		void CrossProduct(Matrix &result,const Vector &V1,const Matrix &M2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(result.Width!=M2.Width)
		throw Vector::BadSize();
	if(result.Height!=3)
		throw Vector::BadSize();
	if(M2.Height!=3)
		throw Vector::BadSize();
	if(V1.size!=3)
		throw Vector::BadSize();
#endif
	for(unsigned i=0;i<result.Width;i++)
	{
		result(0,i)=V1.Values[1]*M2(2,i)-M2(1,i)*V1.Values[2];
		result(1,i)=V1.Values[2]*M2(0,i)-M2(2,i)*V1.Values[0];
		result(2,i)=V1.Values[0]*M2(1,i)-M2(0,i)*V1.Values[1];
	}
}
//------------------------------------------------------------------------------      
/// Multiply Matrix M by vector V1, put the result in vector V2. The Matrix must be of height V2.size and width V1.size.
void Multiply(Vector &V2,const Matrix &M,const Vector &V1)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Width!=V1.size)
		throw Vector::BadSize();
	if(M.Height>V2.size)
		throw Vector::BadSize();
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
			push esi
			xorps xmm6,xmm6
			mov ecx,W
			cmp ecx,4
			jle jTail
			//cmp ecx,3
			//jle jTail
			Dot_Product_Loop:
				movaps xmm1,[eax]
				movaps xmm2,[ebx]
				mulps xmm1,xmm2
				addps xmm6,xmm1
				add eax,16
				add ebx,16
				add ecx,-4
				cmp ecx,4
			jg Dot_Product_Loop
			jTail:
			movaps xmm1,[eax]
			movaps xmm2,[ebx]
			mov ecx,tt      
			movups xmm7,[ecx]
			andps xmm2,xmm7   
			mulps xmm1,xmm2
			addps xmm6,xmm1
			/*cmp ecx,0
			je jFinish
				movss xmm1,[eax]
				movss xmm2,[ebx]
				mulss xmm1,xmm2
				addss xmm6,xmm1
				add eax,4
				add ebx,4
			cmp ecx,1
			je jFinish 
				movss xmm1,[eax]
				movss xmm2,[ebx]
				mulss xmm1,xmm2
				addss xmm6,xmm1
				add eax,4
				add ebx,4
			cmp ecx,2
			je jFinish
				movss xmm1,[eax]
				movss xmm2,[ebx]
				mulss xmm1,xmm2
				addss xmm6,xmm1
				add eax,4
				add ebx,4 
			jFinish:*/
			shufps xmm1,xmm6,0xEE
			shufps xmm1,xmm1,0xEE
			addps xmm6,xmm1
			shufps xmm1,xmm6,0x11
			shufps xmm1,xmm1,0xEE
			addss xmm6,xmm1    
			//jEscape:
			movss [edi],xmm6
			Add edi,4
			mov ecx,MStride
			pop esi
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
			V2.Values[i]+=M(i,j)*V1.Values[j];
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
void MultiplyNegative(Vector &V2,const Matrix &M,const Vector &V1)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Width!=V1.size)
		throw Vector::BadSize();
	if(M.Height>V2.size)
		throw Vector::BadSize();
#endif
#ifdef SIMD
	unsigned MStride=M.W16*4;
	unsigned W=(M.Width+3)>>2;
	float *Result=(float *)(V2.Values);
	//float temp[4];
	//float *tt=temp;
	float *v1=(float *)(V1.Values);
	_asm
	{
		mov ecx,M
		mov edx,[ecx+12]
		cmp edx,0
		je iEscape
		mov edi,dword ptr[Result]
		mov esi,dword ptr[ecx]
		Row_Loop:
			mov ebx,dword ptr[v1]
			mov eax,esi
			push esi
//	mov esi,tt
			xorps xmm6,xmm6
			mov ecx,W
			cmp ecx,0
			je jEscape
			Dot_Product_Loop:
			// Load four values from V2 into xmm0
				movaps xmm1,[eax]
//	movups [esi],xmm1
			// Load four values from V1 into xmm2
				movaps xmm2,[ebx]
//	movups [esi],xmm2
			// Now xmm2 contains the four values from V1.
			// Multiply the four values from M and V1:
				mulps xmm1,xmm2
//	movups [esi],xmm1
			// Now add the four multiples to the four totals in xmm6:
				subps xmm6,xmm1
//	movups [esi],xmm6
			// Move eax to look at the next four values from this row of M:
				add eax,16
			// Move ebx to look at the next four values from V1:
				add ebx,16
				dec ecx
				cmp ecx,0
			jne Dot_Product_Loop
			jEscape:
			// Now add the four sums together:
			shufps xmm1,xmm6,0xEE
//	movups [esi],xmm1
			shufps xmm1,xmm1,0xEE
//	movups [esi],xmm1
			addps xmm6,xmm1
//	movups [esi],xmm6
			shufps xmm1,xmm6,0x11
//	movups [esi],xmm1
			shufps xmm1,xmm1,0xEE
//	movups [esi],xmm1
			addss xmm6,xmm1
//	movups [esi],xmm6
			movss [edi],xmm6
			Add edi,4
			mov ecx,MStride
			pop esi
			add esi,ecx
			dec edx
			cmp edx,0
		jne Row_Loop
		iEscape:
	}
#else
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		V2.Values[i]=0.f;
		for(j=0;j<M.Width;j++)  // j=1,2,(4),17
		{
			V2.Values[i]-=M(i,j)*V1.Values[j];
		}
	}   
#else
//	unsigned W16=(M.Width+3)&~0x3;
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
					vsub vf4,vf4,vf3
					addi t1,16			// next four numbers
					addi t3,-4			// counter-=4
				bgt t3,3,kLoop
					addi t2,16
// Now vf4 contains 4 sums to be added together.
				vaddx vf5,vf4,vf4x
				vaddy vf5,vf4,vf5y
				vaddz vf5,vf4,vf5z  	// So the sum is in the w.
				vmul vf5,vf5,vf0		// Now it's 3 zeroes and the sum in the w slot.
// Now rotate into place.
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
				sub.s $f4,$f4,$f3
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
						: "r" (V2.Values),"r" (M.Values), "r" (V1.Values), "r" (M.Width), "r" (V2.size),
							 "r" (Stride),"r" (M.W16)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#endif
}
//------------------------------------------------------------------------------
void MultiplyAndAdd(Vector &V2,const Matrix &M,const Vector &V1)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Width!=V1.size)
		throw Vector::BadSize();
	if(M.Height!=V2.size)
		throw Vector::BadSize();
#endif        
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		for(j=0;j<M.Width;j++)
		{
			V2.Values[i]+=M(i,j)*V1.Values[j];    //9,12,18,21
		}
	}
#else
/*	static Vector TempV;
	TempV=V2;
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		for(j=0;j<M.Width;j++)
		{
			TempV.Values[i]+=M(i,j)*V1.Values[j];    //9,12,18,21
		}
	}        */
	unsigned MStride=M.W16*4;
	unsigned W=M.Width;
	float *Result=(float *)(V2.Values);
	float *v1=(float *)(V1.Values);
	_asm
	{
		mov ecx,M
		mov edx,[ecx+12]
		cmp edx,0
		je iEscape
		mov edi,dword ptr[Result]
		mov esi,dword ptr[ecx]
		Row_Loop:
			mov ebx,dword ptr[v1]
			mov eax,esi
			push esi
			xorps xmm6,xmm6
			movss xmm6,[edi]
			mov ecx,W
			cmp ecx,0
			jle jEscape
			cmp ecx,3
			jle jTail
			Dot_Product_Loop:
				movaps xmm1,[eax]
				movaps xmm2,[ebx]
				mulps xmm1,xmm2
				addps xmm6,xmm1
				add eax,16
				add ebx,16
				add ecx,-4
				cmp ecx,3
			jg Dot_Product_Loop
			jTail:
			cmp ecx,0
			je jFinish
				movss xmm1,[eax]
				movss xmm2,[ebx]
				mulss xmm1,xmm2
				addss xmm6,xmm1
			cmp ecx,1
			je jFinish 
				movss xmm1,[eax+4]
				movss xmm2,[ebx+4]
				mulss xmm1,xmm2
				addss xmm6,xmm1
			cmp ecx,2
			je jFinish
				movss xmm1,[eax+8]
				movss xmm2,[ebx+8]
				mulss xmm1,xmm2
				addss xmm6,xmm1
			jFinish:
			shufps xmm1,xmm6,0xEE
			shufps xmm1,xmm1,0xEE
			addps xmm6,xmm1
			shufps xmm1,xmm6,0x11
			shufps xmm1,xmm1,0xEE
			addss xmm6,xmm1    
			jEscape:
			movss [edi],xmm6
			Add edi,4
			mov ecx,MStride
			pop esi
			add esi,ecx
			dec edx
			cmp edx,0
		jne Row_Loop
		iEscape:
	}
/*	for(i=0;i<TempV.size;i++)
	{
		if(Fabs(TempV(i)-V2(i))>1e-4f)
		{
			unsigned errorhere=1;
			errorhere++;
			errorhere;
		}
	}    */
#endif
#else
	float *Row1=(M.Values);
	float *Row2=(V1.Values);
	unsigned W=M.Width;
	unsigned W16=(M.Width+3)&~0x3;
	unsigned H=V2.size;
	unsigned Stride=M.W16*4;
	//float *ResultRow=V2.Values;
	asm __volatile__("
		.set noreorder  
		add t4,zero,%4
		blez t4,Escape	// for zero height matrix 
		nop     
		add t9,zero,%0					// Start of V2.Values
		xor t5,t5						// V2's value count
		iLoop:							// Rows of M1 and M
			add t0,zero,t9				// Start of Row i of M
			add t7,zero,%2 				// Row zero of M2
			jLoop:
				add t1,zero,%1			// Start of Row i of M1
				add t3,zero,%3			// Width of M1 and M2
				add t2,zero,t7 			// Row j of M2
				vsub vf4,vf0,vf0
				blt t3,4,Tail
				nop
				kLoop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmul vf3,vf1,vf2
					vadd vf4,vf4,vf3
					addi t1,16			// next four numbers
					addi t2,16
					addi t3,-4			// counter-=4
					bgt t3,3,kLoop
				nop
// Now vf4 contains 4 sums to be added together.
				vaddx vf5,vf4,vf4x
				vaddy vf5,vf4,vf5y
				vaddz vf4,vf4,vf5z  	// So the sum is in the w.
				beqz t3,NoTail
			Tail:
				lqc2 vf1,0(t1)
				lqc2 vf2,0(t2)		// All of next 4 values
				vmul vf2,vf1,vf2
				vaddx vf4,vf4,vf2x	// w of vf1+ x of vf2
				beq t3,1,NoTail
				nop
				vaddy vf4,vf4,vf2y	// w of vf1+ y of vf2
				beq t3,2,NoTail
				nop
				vaddz vf4,vf4,vf2z	// w of vf1+ z of vf2
			NoTail:
				vmul vf4,vf4,vf0		// Now it's 3 zeroes and the sum in the w slot.
// Now rotate into place.	
				and t6,t5,0x3			//	where is the column in 4-aligned data?
				and t8,t0,~0xF			//	4-align the pointer
				beq t6,3,esccase		// W
				nop
					vmr32 vf4,vf4
				beq t6,2,esccase		// Z
				nop
					vmr32 vf4,vf4
				beq t6,1,esccase		// Y
				nop
					vmr32 vf4,vf4		// X
				//b emptycase				// just store, don't add to what's there.
				//nop
			esccase:
				lqc2 vf6,0(t8)
				vadd vf4,vf6,vf4		// add to what's there already.
			emptycase:
				sqc2 vf4,0(t8)			// store in t0
			//blez t3,NoTail
			//Tail:
			//	lwc1 $f4,0(t0)			// Put the sum in f4
			//TailLoop:
			//	lwc1 $f1,0(t1)		// Load 4 numbers into vf1
			//	lwc1 $f2,0(t2)		// Load 4 numbers into vf2
			//	mul.s $f3,$f1,$f2
			//	add.s $f4,$f4,$f3
			//	addi t1,4			// next four numbers
			//	addi t3,-1			// counter-=4
			//bgtz t3,TailLoop
			//	addi t2,4
			//swc1 $f4,0(t0)
			//NoTail:
			addi t9,4					// Next Value of V2
			add %1,%1,%5				// Next Row of M1
			addi t4,-1					// row counter
			addi t5,1					// where in 4-byte aligned V2 vector
		bgtz t4,iLoop
		nop
		Escape:
	"					: 
						: "r" (V2.Values),"r" (Row1), "r" (Row2), "r" (W), "r" (H), "r" (Stride),"r" (W16)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}           
//------------------------------------------------------------------------------
void TransposeMultiplyAndAdd(Vector &V2,const Matrix &M,const Vector &V1)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=V1.size)
		throw Vector::BadSize();
	if(M.Width!=V2.size)
		throw Vector::BadSize();
#endif
	for(i=0;i<M.Width;i++)
	{
		for(j=0;j<M.Height;j++)
		{
			V2.Values[i]+=M(j,i)*V1.Values[j];
		}
	}
}
//------------------------------------------------------------------------------
void TransposeMultiplyAndSubtract(Vector &V2,const Matrix &M,const Vector &V1)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=V1.size)
		throw Vector::BadSize();
	if(M.Width!=V2.size)
		throw Vector::BadSize();
#endif
	for(i=0;i<M.Width;i++)
	{
		for(j=0;j<M.Height;j++)
		{
			V2.Values[i]-=M(j,i)*V1.Values[j];
		}
	}
}
//------------------------------------------------------------------------------
void TransposeMultiply(Vector &V2,const Matrix &M,const Vector &V1)
{                   
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=V1.size)
		throw Vector::BadSize();
	if(M.Width!=V2.size)
		throw Vector::BadSize();
#endif
#ifdef SIMD
	// The Stride is the number of BYTES between successive rows:        
	unsigned MStride=M.W16*4;
	unsigned W=(M.Height+3)>>2;
	const unsigned H=M.Width;
	float *Result=(float *)(V2.Values);
	float *MPtr=M.Values;
	//float *v1=(float *)(V1.Values);
	_asm
	{
		mov edx,H    
		mov edi,dword ptr[Result]
		mov esi,dword ptr[MPtr]//MPtr
		Row_Loop:
			mov ebx,dword ptr[V1]    //v1     
			mov ebx,[ebx+4]
			mov eax,esi
			xorps xmm6,xmm6
			mov ecx,W
			Dot_Product_Loop:
			// Load one value from M into xmm1
				movss xmm1,[eax]
				add eax,MStride
				movss xmm3,[eax]
				add eax,MStride
				movss xmm4,[eax]
				add eax,MStride
				movss xmm5,[eax]
			// now shuffle these last three Values into xmm1:
				shufps xmm1,xmm3,0x44
				shufps xmm1,xmm1,0x88
				shufps xmm4,xmm5,0x44
				shufps xmm4,xmm4,0x88
				shufps xmm1,xmm4,0x44
			// Load four values from V1 into xmm2
				movaps xmm2,[ebx]
			// Now xmm2 contains the four values from V1.
			// Multiply the four values from M and V1:
				mulps xmm1,xmm2
			// Now add the four multiples to the four totals in xmm6:
				addps xmm6,xmm1
			// Move eax to look at the next four values from this column of M:
				//add eax,16
				add eax,MStride
			// Move ebx to look at the next four values from V1:
				add ebx,16
				dec ecx
				cmp ecx,0
			jne Dot_Product_Loop
			// Now add the four sums together:
			shufps xmm1,xmm6,0xEE
			shufps xmm1,xmm1,0xEE
			addps xmm6,xmm1
			shufps xmm1,xmm6,0x11
			shufps xmm1,xmm1,0xEE
			addss xmm6,xmm1   
			movss [edi],xmm6
			Add edi,4
			add esi,4
			dec edx
			cmp edx,0
		jne Row_Loop
	}
#else              
	unsigned i,j;
	for(i=0;i<M.Width;i++)
	{
		V2.Values[i]=0.f;
		for(j=0;j<M.Height;j++)
		{
			V2.Values[i]+=M(j,i)*V1.Values[j];
		}
	}
#endif
}
//------------------------------------------------------------------------------
void Multiply(Vector &result,const Vector &v,const Matrix &M)
{
	unsigned i,j;
	for(i=0;i<M.Width;i++)
	{
		result.Values[i]=0.f;
		for(j=0;j<v.size;j++)
		{
			result.Values[i]+=v.Values[j]*M(j,i);
		}
	}
}                         
//------------------------------------------------------------------------------
void MultiplyVectorByTranspose(Vector &result,const Vector &v,const Matrix &M)
{
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		result.Values[i]=0.f;
		for(j=0;j<v.size;j++)
		{
			result.Values[i]+=v.Values[j]*M(i,j);
		}
	}
}
//------------------------------------------------------------------------------
void MultiplyAndSubtract(Vector &result,const Vector &v,const Matrix &M)
{
	unsigned i,j;
	for(i=0;i<M.Width;i++)
	{
		for(j=0;j<v.size;j++)
		{
			result.Values[i]-=v.Values[j]*M(j,i);
		}
	}
}                                            
//------------------------------------------------------------------------------
void MultiplyTransposeAndSubtract(Vector &V2,const Vector &V1,const Matrix &M)
{                   
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=V2.size)
		throw Vector::BadSize();
	if(M.Width!=V1.size)
		throw Vector::BadSize();
#endif
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		for(j=0;j<V1.size;j++)
		{
			V2.Values[i]-=V1.Values[j]*M(i,j);
		}
	}
}
//------------------------------------------------------------------------------
void MultiplyNegative(Vector &result,const Vector &v,const Matrix &M)
{
	unsigned i,j;
	for(i=0;i<M.Width;i++)
	{
		for(j=0;j<v.size;j++)
			result.Values[i]=-v.Values[j]*M(j,i);
	}
}
//------------------------------------------------------------------------------
void InsertVectorAsRow(Matrix &M,const Vector &v,const unsigned row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=M.Height||row<0||v.size>M.Width)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<v.size;i++)
	{
		M.Values[row*M.W16+i]=v(i);
	}
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
		mov edi,M			// Matrix's Values
		mov ecx,[edi]
		mov eax,[edi+8]		// Put W16 into eax
		shl eax,2			// Multiply by 4 to get the matrix stride
		mov edx,row			// Address of row
		mul edx				// Shift in address from the top to the row
		add ecx,eax			// Put the row's address in ecx
		mov esi,v			// Vector's address
		mov ebx,[esi+4]		// Put Vector's Values in ebx
		mov eax,[esi+8]  
		cmp eax,0
		jle Escape
		Insert_Loop:
			movaps xmm0,[ebx]
			movaps [ecx],xmm0
			add ecx,16      // Move along by 4 floats
			add ebx,16		// Move along by 4 floats
			add eax,-4
			cmp eax,0
		jg Insert_Loop
		Escape:
	}
#endif
}
//------------------------------------------------------------------------------
void InsertVectorAsColumn(Matrix &M,const Vector &v,const unsigned col)
{
	for(unsigned i=0;i<v.size;i++)
		M.Values[i*M.W16+col]=v.Values[i];
}
//------------------------------------------------------------------------------
void InsertNegativeVectorAsRow(Matrix &M,const Vector &v,const unsigned row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=M.Height||row<0)
		throw Vector::BadSize();
#endif
#ifdef SIMD
	_asm
	{
		mov edi,M			// Matrix's Values
		mov ecx,[edi]
		mov eax,[edi+8]		// Put W16 into eax
		shl eax,2			// Multiply by 4 to get the matrix stride
		mov edx,row			// Address of row
		mul edx			// Shift in address from the top to the row
		add ecx,eax			// Put the row's address in ecx
		mov esi,v			// Vector's address
		mov ebx,[esi+4]		// Put Vector's Values in ebx
		mov eax,[edi+16]
		Insert_Loop:       
			xorps xmm1,xmm1
			subps xmm1,[ebx]
			movaps [ecx],xmm1
			add ecx,16      // Move along by 4 floats
			add ebx,16		// Move along by 4 floats
			sub eax,4
			cmp eax,0
		jg Insert_Loop
	}
#else
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<M.Width;i++)
	{
		M.Values[row*M.W16+i]=-v(i);
	}
#else
	asm __volatile__("
		.set noreorder
		add t0,zero,%3
		add t2,zero,%1
		lw t1,0(%0)
		add t3,zero,%2	// t3=row
		lw t4,8(%0)		// t4=M.W16
		muli t4,t4,4
		mul t4,t4,t3	// row*MStride
		add t1,t4
		vsub vf2,vf0,vf0
		vsubw vf2,vf2,vf0w
		Loop:
			lqc2 vf1,0(t2)
			vmul vf1,vf1,vf2
			sqc2 vf1,0(t1)
			addi t2,16
			addi t0,-4
		bgtz t0,Loop
			addi t1,16
		.set reorder
	"					  : 
						  :  "r" (&M),"r"(v.Values), "r" (row),"r"(v.size)
						  :);
#endif
#endif
}
//------------------------------------------------------------------------------
#ifndef SIMD
#ifndef PLAYSTATION2
void SubtractVectorFromRow(Matrix &M,unsigned Row,Vector &V)
{
	for(unsigned i=0;i<M.Width;i++)
		M(Row,i)-=V(i);
}
#else
void SubtractVectorFromRow(Matrix &M,unsigned Row,Vector &V)
{
	float *RowPtr1=&(M.Values[Row*M.W16]);
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of 4-wide columns
		blez t0,Escape				// for zero width matrix 
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2
		ble t0,3,Tail
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			lqc2 vf2,0(t2)			// Load 4 numbers into vf2
			vsub vf1,vf1,vf2
			sqc2 vf1,0(t1)
			addi t0,-1				// counter-=4
			addi t1,16				// next four numbers
		bgt t0,3,iLoop
			addi t2,16
		blez t0,Escape
		Tail:
		tLoop:
			lwc1 f1,0(t1)			// Load a number into f1
			lwc1 f2,0(t2)			// Load a number into f2
			sub.s $f1,$f1,$f2
			swc1 $f1,0(t1)
			addi t1,4				// next four numbers
			addi t0,-1				// counter-=4
		bgtz t0,tLoop
			addi t2,4
		Escape:
	"					: /* Outputs. */
						: /* Inputs */  "r" (M.Width), "r" (RowPtr1), "r" (V.Values)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
}
#endif
#else      
void SubtractVectorFromRow(Matrix &M,unsigned Row,Vector &V)
{
	unsigned Stride=M.W16*4;
	unsigned W=(M.Width+3);
	_asm
	{
		mov edx,M
		mov ebx,V
		mov edi,[edx]
		mov esi,[ebx+4]
		mov eax,Row
		mul Stride
		add edi,eax
		mov eax,W		//[edx+16]
		shr eax,2
		iLoop:
			movaps xmm2,[edi]
			subps xmm2,[esi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
		dec eax
		cmp eax,0
		jne iLoop
	}
}
#endif
//------------------------------------------------------------------------------      
void RowToVector(const Matrix &M,Vector &V,unsigned Row)
{
#ifndef SIMD
#ifndef PLAYSTATION2
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0||V.size<M.Width)
		throw Vector::BadSize();
#endif
	for(unsigned i=0;i<M.Width;i++)
	{
		V(i)=M.Values[Row*M.W16+i];
	}
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
	unsigned Stride=M.W16*4;
	unsigned W=(M.Width+3);
	_asm
	{
		mov edx,M
		mov ebx,V
		mov edi,[edx]
		mov esi,[ebx+4]
		mov eax,Row
		mul Stride
		add edi,eax
		mov eax,W		//[edx+16]
		shr eax,2
		iLoop:
			movaps xmm2,[edi]
			movaps [esi],xmm2
			add edi,16
			add esi,16
		dec eax
		cmp eax,0
		jne iLoop
	}
#endif            
}    
//------------------------------------------------------------------------------
void ColumnToVector(Matrix &M,Vector &V,const unsigned Col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Col>=M.Width||Col<0||M.Height>V.size)
		throw Vector::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M.Height;i++)
		V.Values[i]=M.Values[i*M.W16+Col];
#else
	float *Ptr1=&(M.Values[Col]);
	float *Ptr2=V.Values;
	asm __volatile__("
		.set noreorder
		lw t0,12(%2)				// t0= number of rows
		blez t0,Escape				// for zero width matrix
		add t3,zero,%3
		muli t3,t3,4
		add t1,zero,%0				// t1= address of matrix 1's row
		add t2,zero,%1
		iLoop:
			lwc1 $f1,0(t1)			// Load 4 numbers into vf1
			swc1 $f1,0(t2)          // Load 4 numbers into vector
			addi t2,4				// next four numbers
			addi t0,-1				// counter-=1
		bgtz t0,iLoop
			add t1,t3
		Escape:
	"					: /* Outputs. */
						: /* Inputs */  "r" (Ptr1), "r" (Ptr2),"r" (&M),"r" (M.W16)
						: /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	if(V.size==3)
	{
		_asm
		{
			mov ebx,M
			mov edx,V
			mov esi,[ebx]		//Values
			mov edi,[edx+4]		//v.Values
			movaps xmm1,[esi]    
			movaps xmm2,[esi+16]
			movaps xmm3,[esi+32]
			cmp Col,2
			je case2
			cmp Col,1
			je case1

			movss [edi],xmm1
			movss [edi+4],xmm2
			movss [edi+8],xmm3
			jmp case3

			ret
			case1:

			shufps xmm1,xmm1,21
			shufps xmm2,xmm2,21
			shufps xmm3,xmm3,21
			movss [edi],xmm1
			movss [edi+4],xmm2
			movss [edi+8],xmm3   
			jmp case3

			ret
			case2:

			shufps xmm1,xmm1,42
			shufps xmm2,xmm2,42
			shufps xmm3,xmm3,42
			movss [edi],xmm1
			movss [edi+4],xmm2
			movss [edi+8],xmm3  
			case3:

			/*mov eax,col
			imul eax,4
			add esi,eax
			fld [esi]
			fstp [edi]   
			fld [esi+16]
			fstp [edi+4]
			fld [esi+32]
			fstp [edi+8]  */
		}
		return;
	}
	for(unsigned i=0;i<M.Height;i++)
		V.Values[i]=M.Values[i*M.W16+Col];
#endif
}
//------------------------------------------------------------------------------   
#ifndef PLAYSTATION2
#ifndef SIMD
void InsertCrossProductAsRow(Matrix &M,const Vector &V1,const Vector &V2,const unsigned Row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector::BadSize();
#endif
	M.Values[Row*M.W16+0]=V1.Values[1]*V2.Values[2]-V2.Values[1]*V1.Values[2];
	M.Values[Row*M.W16+1]=V1.Values[2]*V2.Values[0]-V2.Values[2]*V1.Values[0];
	M.Values[Row*M.W16+2]=V1.Values[0]*V2.Values[1]-V2.Values[0]*V1.Values[1];
}
#else
void InsertCrossProductAsRow(Matrix &M,const Vector &V1,const Vector &V2,const unsigned Row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector::BadSize();
#endif
	unsigned MStride=4*M.W16;
	_asm
	{                
		mov esi,M			// Vector's Values
		mov ecx,[esi]		// Put Vector's Values in ebx
		mov eax,Row			//
		mul MStride
		add ecx,eax
		mov esi,V1			// Vector's Values
		mov eax,[esi+4]		// Put Vector's Values in ebx
		mov esi,V2			// Vector's Values
		mov ebx,[esi+4]		// Put Vector's Values in ebx
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
}
#endif
#else
void InsertCrossProductAsRow(Matrix &M,const Vector &V1,const Vector &V2,const unsigned Row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector::BadSize();
#endif
	Row;
	asm __volatile__("  
			.set noreorder
			lqc2 vf01,0(%1)		// Load v1's float address into vf1
			lqc2 vf02,0(%2)		// Load v2's float address into vf1
			lw t0,0(a0)
			lw t1,8(a0)			// M.W16
			vopmula ACC,vf01,vf02
			muli t1,t1,4		// W16*4 = bytes per row
			mul t1,t1,a3			// Stride=4*W16*Row
			vopmsub vf03,vf02,vf01
			add t0,t1			// Address of row
			sqc2 vf03,0(t0)		//  and get the result from vf03
		"					: /* Outputs. */ 
							: /* Inputs */ "r" (&M), "r" (V1.Values), "r" (V2.Values)
							: /* Clobber */ "$vf1", "$vf2", "$vf3");
}
#endif
//------------------------------------------------------------------------------ 
float MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector &V)
{
#ifndef SIMD 
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector::BadSize();
#endif
#ifndef PLAYSTATION2
	float sum=0;
	unsigned i;
	for(i=0;i<M.Width;i++)
	{
		sum+=M.Values[Row*M.W16+i]*V(i);
	}
	return sum;
#else
	float *Row1=&(M.Values[Row*M.W16]);
	asm __volatile__("
		.set noreorder     
		blez %0,Escape     
		nop
		sub.s f4,f4,f4
		ble %0,3,Loop2
		nop
		vsub vf4,vf0,vf0
		Loop:
			addi %0,-4			// counter-=4
			lqc2 vf1,0(%1)		// Load 4 numbers into vf1
			lqc2 vf2,0(%2)		// Load 4 numbers into vf2
			vmul vf3,vf1,vf2
			vadd vf4,vf4,vf3
			addi %1,16			// next four numbers
		bgt %0,3,Loop
			addi %2,16  
// Now vf4 contains 4 sums to be added together.
		vaddw vf5,vf4,vf4w
		vaddz vf5,vf4,vf5z
		vaddy vf5,vf4,vf5y
		sqc2 vf5,0(%3)
        blez %0,Escape
        nop
		lwc1 f4,0(%3)
		Loop2:
			lwc1 f1,0(%1)		// Load a number into vf1
			lwc1 f2,0(%2)		// Load a number into vf2
			mul.s f3,f1,f2
			add.s f4,f4,f3
			addi %1,4			// next four numbers  
			addi %0,-1			// counter-=4
		bgtz %0,Loop2
			addi %2,4
		swc1 f4,0(%3)
		xor t0,t0
		sw t0,12(%3)
		Escape:
	"					:  
						:  "g"(M.Width),"g"(Row1),"g"(V.Values),"g"(TempVector3b.Values)
						:  "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
	return TempVector3b.Values[0];
#endif
#else
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector::BadSize();
#endif       
	float temp;                   
	float *t3=&temp;
	//static float tt[4];
	//float *ttt=tt;             x
	_asm
	{
		mov edi,M			// matrix's Values
		mov ecx,[edi]
		mov eax,[edi+8]		// Put W16 into eax
		shl eax,2			// Multiply by 4 to get the matrix stride
		mov ebx,Row			// Address of row
		mul ebx			// Shift in address from the top to the row
		add ecx,eax			// Put the row's address in ecx
		mov esi,V			// Vector's Values
		mov ebx,[esi+4]		// Put Vector's Values in ebx
		mov eax,[edi+16]    // Use Width, not W16.
//mov edx,ttt
		xorps xmm0,xmm0
		cmp eax,3
		jle TailLoop
		i_Loop:
			movaps xmm1,[ebx]
//movups [edx],xmm1
			mulps xmm1,[ecx]
//movups [edx],xmm1
			addps xmm0,xmm1
//movups [edx],xmm0
			add ecx,16      // Move along by 4 floats
			add ebx,16		// Move along by 4 floats
			sub eax,4
			cmp eax,3
		jg i_Loop
		cmp eax,0
		jle NoTail
		TailLoop:
			movss xmm1,[ebx]
//movups [edx],xmm1
			mulss xmm1,[ecx]
//movups [edx],xmm1
			addss xmm0,xmm1
//movups [edx],xmm0
			add ecx,4		// Move along by 4 floats
			add ebx,4		// Move along by 4 floats
			sub eax,1
			cmp eax,0
		jg TailLoop      
		mov eax,[edi+16]    // Use Width, not W16.
		cmp eax,4
		jl TailOnly
		NoTail:
//movups [edx],xmm0
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movaps xmm2,xmm0
		shufps xmm2,xmm2,14  // 14 = 2+4+8
//movups [edx],xmm2
		// Add them together, we're only interested in the lower results
		addps xmm0,xmm2     
//movups [edx],xmm0
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movaps xmm2,xmm0
		shufps xmm2,xmm2,1   
//movups [edx],xmm2
		addss xmm0,xmm2
//movups [edx],xmm0
		TailOnly:
		mov eax,t3
		movss [eax],xmm0
	}
	return temp;
#endif
}     
//------------------------------------------------------------------------------
void AddFloatTimesRow(Vector &V1,float Multiplier,Matrix &M2,unsigned Row2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M2.Width;i++)
		V1(i)+=Multiplier*M2(Row2,i);
#else
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	asm __volatile__("
		.set noreorder 
		add t0,zero,%0
		blez t0,Escape				// for zero width matrix
		lwc1 $f3,0(%4)
		swc1 $f3,0(%3)
		lqc2 vf4,0(%3)
		vsub vf3,vf00,vf00
		ble t0,3,iLoop2				// for <4    
		vaddx vf3,vf3,vf4x			// All 4 numbers are equal to multiplier
		iLoop:
			addi t0,-4				// counter-=4
			lqc2 vf1,0(%1)			// Load 4 numbers into vf1
			lqc2 vf2,0(%2)			// Load 4 numbers into vf2
			vmul vf4,vf2,vf3
			vadd vf1,vf1,vf4
			sqc2 vf1,0(%1)
			addi %1,16				// next four numbers
		bgt t0,3,iLoop
			addi %2,16
		blez t0,Escape				// for zero width matrix   
		iLoop2:
			addi t0,-1				// counter-=4
			lwc1 f1,0(%1)			// Load 4 numbers into vf1
			lwc1 f2,0(%2)			// Load 4 numbers into vf2
			mul.s f4,f2,f3
			add.s f1,f1,f4
			swc1 f1,0(%1)
			addi %1,4				// next four numbers
		bgtz t0,iLoop2
			addi %2,4
		Escape:
	"					: 
						: "g" (M2.Width), "g" (V1.Values), "g" (RowPtr2),"g" (TempVector3.Values),"g"(&Multiplier)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	unsigned Stride2=M2.W16*4;
	_asm
	{
		mov ecx,V1
		mov ebx,M2
		mov edi,[ecx+4]
		mov esi,[ebx]
		mov eax,Row2
		mul Stride2
		add esi,eax
		mov eax,[ebx+16]
		cmp eax,0
		jz iEscape  
		movss xmm0,Multiplier     
		cmp eax,4
		jl iLoop2
		shufps xmm0,xmm0,0
		iLoop:
			movaps xmm2,[esi]
			mulps xmm2,xmm0
			addps xmm2,[edi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
			add eax,-4
			cmp eax,3
		jg iLoop
        cmp eax,0
        jle iEscape
		iLoop2:
			movss xmm2,[esi]
			mulss xmm2,xmm0
			addss xmm2,[edi]
			movss [edi],xmm2
			add edi,4
			add esi,4
			add eax,-1
			cmp eax,0
		jg iLoop2
		iEscape:
	}
#endif            
}                   
//------------------------------------------------------------------------------
void AddFloatTimesColumn(Vector &V1,float Multiplier,Matrix &M2,unsigned Col2)
{
	for(unsigned i=0;i<M2.Height;i++)  
		V1(i)+=Multiplier*M2(i,Col2);
}
void Multiply(Matrix &M,const Vector &V1,const Matrix &M2,unsigned Column)
{                 
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<M.Height;i++)
		for(j=0;j<V1.size;j++)
			M(i,Column+j)=V1(j)*M2(i,Column+j);
#else
	float *Result=&(M.Values[Column]);
	float *Source=&(M2.Values[Column]);
	unsigned Stride2=M2.W16*4;
	unsigned RStride=M.W16*4;
	asm __volatile__("
		.set noreorder
		beqz %7,Escape
		nop
		add t7,zero,%7
		beqz %4,Escape
		nop
		and t1,%0,3
		bnez t1,Unaligned				// Is the result aligned on a 16-byte boundary?
		nop
		bne %4,6,Large
		Size8:
		lqc2 vf1,0(%1)	
		lqc2 vf2,16(%1)
		iLoop8:
			add t2,zero,%2
			add t3,zero,%3
				lqc2 vf11,0(t2)	
				lqc2 vf12,16(t2)
				vmul vf21,vf1,vf11
				vmul vf22,vf2,vf12
				sqc2 vf21,0(t3)
				sqc2 vf22,16(t3)
			add %2,%5
			add %3,%6
			addi %7,-1
		bgtz %7,iLoop8
		nop
		b Escape
		Large:
		iLoopg:
			add t1,zero,%1
			add t2,zero,%2
			add t3,zero,%3
			add t4,zero,%4
			jLoopg:
				lqc2 vf1,0(t1)			// Load 4 numbers into vf1
				lqc2 vf2,0(t2)			// Load 4 numbers into vf2
				vmul vf3,vf1,vf2
				sqc2 vf3,0(t3)
				addi t1,16				// next four numbers
				addi t2,16				// next four numbers
				addi t4,-4				// counter-=4
			bgtz t4,jLoopg
				addi t3,16
			add %2,%5
			add %3,%6
			addi t7,-1
		bgtz t7,iLoopg
		nop
		b Escape
		Unaligned:
		iLoopU:
			add t1,zero,%1
			add t2,zero,%2
			add t3,zero,%3
			add t4,zero,%4
			jLoopU:
				lwc1 f1,0(t1)			// Load 4 numbers into vf1
				lwc1 f2,0(t2)			// Load 4 numbers into vf2
				mul.s f3,f1,f2
				swc1 f3,0(t3)
				addi t1,4				// next four numbers
				addi t3,4
				addi t4,-1				// counter-=4
			bgtz t4,jLoopU
				addi t2,4
			add %2,%5
			add %3,%6
			addi t7,-1
		bgtz t7,iLoopU
		nop
		Escape:
	"					:
						: "g" (Column), "g" (V1.Values), "g" (Source), "g" (Result),"g" (V1.size),
							"g" (Stride2),"g"(RStride),"g"(M.Height)
						: );
#endif
#else     
	unsigned i,j;
	for(i=0;i<M.Height;i++)
		for(j=0;j<V1.size;j++)
			M(i,Column+j)=V1(j)*M2(i,Column+j);
#endif
}
//--------------------------------------------------------------------------- 
void SolveFromCholeskyFactors(Vector &R,Matrix &Fac,Vector &X)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Fac.Height!=X.size||Fac.Height!=R.size)
	{
		throw Vector::BadSize();
	}
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	int i,j;
	for(i=0;i<(int)Fac.Height;i++)
	{
		float sum=X.Values[i];
		for(j=0;j<i;j++)
			sum-=Fac.Values[i*Fac.W16+j]*R.Values[j];
		sum/=Fac.Values[i*Fac.W16+i];
		R.Values[i]=sum;
	}
	for(i=(int)Fac.Height-1;i>=0;i--)
	{
		float sum=R.Values[i];
		for(j=i+1;j<(int)Fac.Height;j++)
			sum-=Fac.Values[j*Fac.W16+i]*R.Values[j];
		sum/=Fac.Values[i*Fac.W16+i];
		R.Values[i]=sum;
	}
#else
	unsigned FacStride=Fac.W16*4;  
	asm __volatile__("
		.set noreorder
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add s0,zero,%3
		blez t0,Escape					// for zero width matrix
		xor t3,t3						// s0=0
		iLoop1:
			and t4,t1,~15
			lqc2 vf1,0(t4)
			and t8,t1,15
			beq t8,12,esccase	// W
			nop
			beq t8,0,esccase	// X
				vmr32 vf1,vf1
			beq t8,4,esccase	// Y
				vmr32 vf1,vf1
			vmr32 vf1,vf1		// Z
			esccase:       
			vmul vf1,vf1,vf0	// vf1=0,0,0,v1.Values(i)
                            
			add t4,zero,t3		// k=0
			beqz t4,NoTail1
			add s3,zero,s0
			ble t4,3,kEscape1
			add s2,zero,%2
			kLoop0:
				lqc2 vf2,0(s2)
				lqc2 vf3,0(s3)
				vmul vf2,vf2,vf3
				vsub vf1,vf1,vf2
				addi s2,16
				addi t4,-4
			bgt t4,3,kLoop0
				addi s3,16
// Now vf1 contains 4 sums to be added together.
			vaddx vf5,vf1,vf1x
			vaddy vf5,vf1,vf5y
			vaddz vf1,vf1,vf5z
		kEscape1:
			beqz t4,NoTail1
			lqc2 vf2,0(s2)
			lqc2 vf3,0(s3)		// All of next 4 values
			vmul vf2,vf2,vf3
			addi s3,4
			beq t4,1,NoTail1
			vsubx vf1,vf1,vf2x	// w of vf1+ x of vf2   
			addi s3,4
			beq t4,2,NoTail1
			vsuby vf1,vf1,vf2y	// w of vf1+ y of vf2  
			addi s3,4
			vsubz vf1,vf1,vf2z	// w of vf1+ z of vf2
		NoTail1:
			sqc2 vf1,0(%5)
			lwc1 f1,12(%5)
			lwc1 f2,0(s3)
			div.s f3,f1,f2
			swc1 f3,0(t2)
			addi t3,1  
			addi t1,4
			add s0,%6
		blt t3,%0,iLoop1
			addi t2,4
		Escape:
		
		add t0,zero,%0
		add t1,zero,%2
		
		addi t0,-1
		muli t0,t0,4
		add t1,t1,t0
		
		add s0,zero,%3
		add t0,zero,%0
		addi t0,-1
		addi t2,%6,4
		mul t0,t0,t2
		add s0,s0,t0
		add t0,zero,%0
		blez t0,Escape2					// for zero width matrix
		add t3,%0,-1					// t3= Height-1
		iLoop2:
			lwc1 f1,0(t1)
                            
			addi t4,t3,1		// j=i+1
			beq t4,%0,NoTail2
			
			add s3,s0,%6		// Fac(j,i)

			add s2,t1,4			// R.Values(i)
			kLoop2:
				lwc1 f2,0(s2)
				lwc1 f3,0(s3)
				mul.s f4,f2,f3
				sub.s f1,f1,f4
				addi s2,4
				addi t4,1
			blt t4,%0,kLoop2
				add s3,%6
		NoTail2:
			lwc1 f2,0(s0)
			div.s f3,f1,f2
			swc1 f3,0(t1)
			addi t3,-1  
			addi t1,-4
			sub s0,%6
			addi s0,-4			// s0=Fac(i,i)
		bgez t3,iLoop2
			nop
		Escape2:
	"					:
						: "r"(Fac.Height),"r" (X.Values),"r" (R.Values),"r"(Fac.Values),"r"(Fac.W16),
							"r"(TempVector3.Values),"r"(FacStride)
						: "s0","s1","s2","s3");
#endif     
#else
	unsigned FacStride=Fac.W16*4;
	unsigned H=Fac.Height;
	_asm
	{
		mov esi,Fac
		mov ecx,[esi]			// Fac.Values
		mov edi,X
		mov edi,[edi+4]			// X.Values
		mov eax,0
		mov esi,R
		mov esi,[esi+4]			// R.Values
		iLoop1:         
			push esi
			xorps xmm1,xmm1
			movss xmm1,[edi]
			mov esi,R
			mov esi,[esi+4]
			mov edx,ecx			// Fac.Values[i]
			mov ebx,eax
			cmp ebx,1
			jl Escape1
			cmp ebx,4
			jl Tail1
			jLoop1:     
				movaps xmm2,[edx]
				mulps xmm2,[esi]
				subps xmm1,xmm2
				add edx,16
				add esi,16
				add ebx,-4
				cmp ebx,3
			jg jLoop1
			Tail1:
			cmp ebx,0
			je Escape1
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2   
				add edx,4
			cmp ebx,1
			je Escape1
				add esi,4
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2  
				add edx,4
			cmp ebx,2
			je Escape1
				add esi,4
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2   
				add edx,4
			Escape1:         
			movhlps xmm2,xmm1
			addps xmm1,xmm2         // X+Z,Y+W,--,--
			movaps xmm2,xmm1
			shufps xmm2,xmm2,1      // Y+W,--,--,--
			addss xmm1,xmm2
			pop esi       
			movss xmm3,[edx]
			divss xmm1,xmm3
			movss [esi],xmm1
			add ecx,FacStride
			add esi,4
			add edi,4
			add eax,1
			cmp eax,H
		jl iLoop1
		add esi,-4
		sub ecx,FacStride
		mov edx,H
		add edx,-1
		imul edx,4
		add ecx,edx			// Fac(i,i)
		iLoop2:
			xorps xmm1,xmm1
			movss xmm1,[esi]
			mov edi,esi
			add edi,4
			mov edx,ecx			// Fac.Values[i]
			add edx,FacStride
			mov ebx,eax
			cmp ebx,H
			jge Escape2
			jLoop2:
				movss xmm2,[edx]
				mulss xmm2,[edi]
				subss xmm1,xmm2
				add edx,FacStride
				add edi,4
				add ebx,1
				cmp ebx,H
			jl jLoop2
			Escape2:
			movss xmm3,[ecx]
			divss xmm1,xmm3
			movss [esi],xmm1
			sub ecx,FacStride
			add ecx,-4           	// Fac(i,i)
			add esi,-4
			add eax,-1
			cmp eax,0
		jg iLoop2
	};
/*	for(i=Fac.Height-1;i>=0;i--)
	{
		float sum=R.Values[i];
		for(j=i+1;j<Fac.Height;j++)
			sum-=Fac.Values[j*Fac.W16+i]*R.Values[j];
		sum/=Fac.Values[i*Fac.W16+i];
		R.Values[i]=sum;
	}   */
#endif
}                           
//--------------------------------------------------------------------------- 
void SolveFromLowerTriangular(Vector &R,Matrix &Fac,Vector &X)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Fac.Height!=X.size||Fac.Height!=R.size)
	{
		throw Vector::BadSize();
	}
#endif
#if 1//ndef SIMD
#if 1//ndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<Fac.Height;i++)
	{
		float sum=X.Values[i];
		for(j=0;j<i;j++)
			sum-=Fac.Values[i*Fac.W16+j]*R.Values[j];
		sum/=Fac.Values[i*Fac.W16+i];
		R.Values[i]=sum;
	}
#else
	unsigned FacStride=Fac.W16*4;
	asm __volatile__("
		.set noreorder
		add t0,zero,%0
		add t1,zero,%1
		add t2,zero,%2
		add s0,zero,%3
		blez t0,Escape					// for zero width matrix
		xor t3,t3						// s0=0
		iLoop1:
			and t4,t1,~15
			lqc2 vf1,0(t4)
			and t8,t1,15
			beq t8,12,esccase	// W
			nop
			beq t8,0,esccase	// X
				vmr32 vf1,vf1
			beq t8,4,esccase	// Y
				vmr32 vf1,vf1
			vmr32 vf1,vf1		// Z
			esccase:       
			vmul vf1,vf1,vf0	// vf1=0,0,0,v1.Values(i)
                            
			add t4,zero,t3		// k=0
			beqz t4,NoTail1
			add s3,zero,s0
			ble t4,3,kEscape1
			add s2,zero,%2
			kLoop0:
				lqc2 vf2,0(s2)
				lqc2 vf3,0(s3)
				vmul vf2,vf2,vf3
				vsub vf1,vf1,vf2
				addi s2,16
				addi t4,-4
			bgt t4,3,kLoop0
				addi s3,16
// Now vf1 contains 4 sums to be added together.
			vaddx vf5,vf1,vf1x
			vaddy vf5,vf1,vf5y
			vaddz vf1,vf1,vf5z
		kEscape1:
			beqz t4,NoTail1
			lqc2 vf2,0(s2)
			lqc2 vf3,0(s3)		// All of next 4 values
			vmul vf2,vf2,vf3
			addi s3,4
			beq t4,1,NoTail1
			vsubx vf1,vf1,vf2x	// w of vf1+ x of vf2   
			addi s3,4
			beq t4,2,NoTail1
			vsuby vf1,vf1,vf2y	// w of vf1+ y of vf2  
			addi s3,4
			vsubz vf1,vf1,vf2z	// w of vf1+ z of vf2
		NoTail1:
			sqc2 vf1,0(%5)
			lwc1 f1,12(%5)
			lwc1 f2,0(s3)
			div.s f3,f1,f2
			swc1 f3,0(t2)
			addi t3,1  
			addi t1,4
			add s0,%6
		blt t3,%0,iLoop1
			addi t2,4
		Escape:
	"					:
						: "r"(Fac.Height),"r" (X.Values),"r" (R.Values),"r"(Fac.Values),"r"(Fac.W16),
							"r"(TempVector3.Values),"r"(FacStride)
						: "s0","s1","s2","s3");
#endif     
#else
	unsigned FacStride=Fac.W16*4;
	unsigned H=Fac.Height;
	_asm
	{
		mov esi,Fac
		mov ecx,[esi]			// Fac.Values
		mov edi,X
		mov edi,[edi+4]			// X.Values
		mov eax,0
		mov esi,R
		mov esi,[esi+4]			// R.Values
		iLoop1:         
			push esi
			xorps xmm1,xmm1
			movss xmm1,[edi]
			mov esi,R
			mov esi,[esi+4]
			mov edx,ecx			// Fac.Values[i]
			mov ebx,eax
			cmp ebx,1
			jl Escape1
			cmp ebx,4
			jl Tail1
			jLoop1:     
				movaps xmm2,[edx]
				mulps xmm2,[esi]
				subps xmm1,xmm2
				add edx,16
				add esi,16
				add ebx,-4
				cmp ebx,3
			jg jLoop1
			Tail1:
			cmp ebx,0
			je Escape1
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2   
				add edx,4
			cmp ebx,1
			je Escape1
				add esi,4
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2  
				add edx,4
			cmp ebx,2
			je Escape1
				add esi,4
				movss xmm2,[edx]
				mulss xmm2,[esi]
				subss xmm1,xmm2   
				add edx,4
			Escape1:         
			movhlps xmm2,xmm1
			addps xmm1,xmm2         // X+Z,Y+W,--,--
			movaps xmm2,xmm1
			shufps xmm2,xmm2,1      // Y+W,--,--,--
			addss xmm1,xmm2
			pop esi       
			movss xmm3,[edx]
			divss xmm1,xmm3
			movss [esi],xmm1
			add ecx,FacStride
			add esi,4
			add edi,4
			add eax,1
			cmp eax,H
		jl iLoop1
	};
/*	for(i=Fac.Height-1;i>=0;i--)
	{
		float sum=R.Values[i];
		for(j=i+1;j<Fac.Height;j++)
			sum-=Fac.Values[j*Fac.W16+i]*R.Values[j];
		sum/=Fac.Values[i*Fac.W16+i];
		R.Values[i]=sum;
	}   */
#endif
}

#define pi 3.1415926536f
Vector GetAircraftFromMatrix(Matrix &T)
{
	Vector angles(0,0,0);
	float val1,val2;
	val1=-T(0,1);
	val2=T(1,1);
	if(Fabs(val1)>1e-6||Fabs(val2)>1e-6)
		angles(0)=(float)InverseTangent(val1,val2);
	else
		angles(0)=0;
	val1=-T(2,0);
	val2=T(2,2);
	if(val1!=0||val2!=0)
		angles(2)=(float)InverseTangent(val1,val2);
	val1=T(2,1);
	float cos0,sin0;
	cos0=cos(angles(0));
	sin0=sin(angles(0));
	if(Fabs(cos0)>1e-1f)
		val2=T(1,1)/cos0;
	else
		val2=-T(0,1)/sin0;
	if(Fabs(val1)>1e-6||Fabs(val2)>1e-6)
		angles(1)=(float)InverseTangent(val1,val2); 
	else
		angles(1)=0;
	if(angles(1)>pi/2)
	{
		angles(1)=pi-angles(1);
		angles(0)-=pi;
	}
	if(angles(1)<-pi/2)
	{
		angles(1)=-pi-angles(1);
		angles(0)-=pi;
	}
	while(angles(0)>pi)
		angles(0)-=2*pi;
	while(angles(0)<-pi)
		angles(0)+=2*pi;
	return angles;
}     
//    
}
}
