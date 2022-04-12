#define SIM_MATH

#include <string.h>
#include <stdio.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include "DebugNew.h"			// Enable Visual Studio's memory debugging
#endif
#include <algorithm>

#include "Matrix.h"  
#include "Vector3.h"
#include "MatrixVector3.h"

#ifdef PLAYSTATION_2
	//#define PLAYSTATION2
#endif

#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#endif

using namespace platform::math;

void platform::math::Precross(Matrix4x4 &M,const Vector3 &v)
{
#ifndef PLAYSTATION2
#ifndef SIMD
								M.Values[0*4+1]=-v.Values[2];M.Values[0*4+2]=v.Values[1];
	M.Values[1*4+0]=v.Values[2];								M.Values[1*4+2]=-v.Values[0];
	M.Values[2*4+0]=-v.Values[1];M.Values[2*4+1]=v.Values[0];
#else
	__asm
	{
		mov edi,M
		mov esi,v
		xorps xmm2,xmm2
		xorps xmm3,xmm3
		xorps xmm4,xmm4
		movss xmm1,[esi+4]
		movss [edi+8],xmm1
		subss xmm2,xmm1
		movss [edi+32],xmm2
		movss xmm1,[esi+8]
		movss [edi+16],xmm1
		subss xmm3,xmm1
		movss [edi+4],xmm3
		movss xmm1,[esi]
		movss [edi+36],xmm1
		subss xmm4,xmm1
		movss [edi+24],xmm4
	};
/*	asm
	{
		mov eax,M 
		mov edi,[eax]
		mov ebx,v
		mov esi,[ebx]
		fld [esi+4]
		fst [edi+8]
		fchs
		fstp [edi+32]
		fld [esi+8]
		fst [edi+16]
		fchs
		fstp [edi+4]
		fld [esi]
		fst [edi+36]
		fchs
		fstp [edi+24]
	};    */
#endif
#else
								M.Values[0*4+1]=-v.Values[2];M.Values[0*4+2]=v.Values[1];
	M.Values[1*4+0]=v.Values[2];								M.Values[1*4+2]=-v.Values[0];
	M.Values[2*4+0]=-v.Values[1];M.Values[2*4+1]=v.Values[0];
#endif
}
//------------------------------------------------------------------------------
void platform::math::TransposeMultiply(Vector3 &V2,const Matrix &M,const Vector3 &V1)
{                   
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=3)
		throw Vector3::BadSize();
	if(M.Width!=3)
		throw Vector3::BadSize();
#endif
#ifdef SIMD
	// The Stride is the number of BYTES between successive rows:        
	unsigned MStride=4*M.W16;
	const unsigned H=M.Width;
	float *Result=(float *)(V2.Values);
	float *MPtr=M.Values;
	//float *v1=(float *)(V1.Values);
	_asm
	{
		mov edx,H    
		mov edi,dword ptr[Result]
		mov esi,dword ptr[MPtr]		//MPtr
		Row_Loop:
			mov ebx,dword ptr[V1]	//v1     
			//mov ebx,[ebx+4]
			mov eax,esi
			xorps xmm6,xmm6
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
void platform::math::Multiply3(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{
	Vector3 res;
	res.Values[0]=M(0,0)*V1.Values[0]+M(0,1)*V1.Values[1]+M(0,2)*V1.Values[2];
	res.Values[1]=M(1,0)*V1.Values[0]+M(1,1)*V1.Values[1]+M(1,2)*V1.Values[2];
	res.Values[2]=M(2,0)*V1.Values[0]+M(2,1)*V1.Values[1]+M(2,2)*V1.Values[2];
	V2=res;
}

void platform::math::Multiply3(Vector3 &V2,const Vector3 &V1,const Matrix4x4 &M)
{
	Vector3 res;
	res.x=M(0,0)*V1.x;
	res.x+=M(1,0)*V1.y;
	res.x+=M(2,0)*V1.z;
	res.y=M(0,1)*V1.x+M(1,1)*V1.y+M(2,1)*V1.z;
	res.z=M(0,2)*V1.x+M(1,2)*V1.y+M(2,2)*V1.z;
	V2=res;
}

void platform::math::Multiply4(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{              
#ifndef SIMD
//	V1.Values[3]=1;
	V2.Values[0]=M(0,0)*V1.Values[0]+M(0,1)*V1.Values[1]+M(0,2)*V1.Values[2]+M(0,3);
	V2.Values[1]=M(1,0)*V1.Values[0]+M(1,1)*V1.Values[1]+M(1,2)*V1.Values[2]+M(1,3);
	V2.Values[2]=M(2,0)*V1.Values[0]+M(2,1)*V1.Values[1]+M(2,2)*V1.Values[2]+M(2,3);  
#else       
	//V1.Values[3]=1;
	static float v[]={0,0,0,1.f};
	_asm
	{
		mov ecx,M
		movups xmm0,v
		mov eax,V1
		movaps xmm1,[eax]
		mov ebx,V2
		movaps xmm3,[ecx]				// load matrix
		movaps xmm4,[ecx+16]
		movaps xmm5,[ecx+32]
		addps xmm1,xmm0
		movaps xmm2,xmm1
		shufps xmm2,xmm2,192		// x x x w
		movaps xmm6,xmm1
		shufps xmm6,xmm6,213		// y y y w
		shufps xmm1,xmm1,234		// z z z w
		mulps xmm2,xmm3   
		mulps xmm6,xmm4
		mulps xmm1,xmm5
		addps xmm2,xmm6
		addps xmm2,xmm1
		movaps [ebx],xmm2
	}
#endif
}
//------------------------------------------------------------------------------
void platform::math::MultiplyAndAdd(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{ 
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i,j;
	for(i=0;i<3;i++)
	{
		for(j=0;j<3;j++)
		{
			V2.Values[i]+=M(i,j)*V1.Values[j];    //9,12,18,21
		}
	}
#else
	unsigned MStride=4*4;
	unsigned W=4;
	float *Result=(float *)(V2.Values);
	float *v1=(float *)(V1.Values);
	_asm
	{
		mov ecx,M
		mov edx,4
		cmp edx,0
		je iEscape
		mov edi,V2
		mov esi,M
		Row_Loop:
			mov ebx,V1
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
	float *Row2=(float *)(V1.Values);
	unsigned W=M.Width;
	unsigned W16=(M.Width+3)&~0x3;
	unsigned H=3;
	unsigned Stride=4*4;
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
void platform::math::MultiplyAndSubtract(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			V2.Values[i]-=M(i,j)*V1.Values[j];
		}
	}
#else
	float *Row1=(M.Values);
	float *Row2=(float *)(V1.Values);
	unsigned W16=(M.Width+3)&~0x3;
	unsigned H=V2.size;
	unsigned Stride=4*4;
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
				blt t3,4,Tail
				vsub vf4,vf0,vf0
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
				vaddz vf5,vf4,vf5z  	// So the sum is in the w.
				vmul vf5,vf5,vf0		// Now it's 3 zeroes and the sum in the w slot.
// Now rotate into place.	
				and t6,t5,0x3			//	where is the column in 4-aligned data?
				and t8,t0,~0xF			//	4-align the pointer
				beq t6,3,esccase		// W
				nop
					vmr32 vf5,vf5
				beq t6,2,esccase		// Z
				nop
					vmr32 vf5,vf5
				beq t6,1,esccase		// Y
				nop
					vmr32 vf5,vf5		// X
				//b emptycase				// just store, don't add to what's there.
				//nop	
			esccase:
				lqc2 vf6,0(t8)
				vsub vf6,vf6,vf5		// add to what's there already.
			emptycase:
				sqc2 vf6,0(t8)			// store in t0
			blez t3,NoTail
			Tail:
				lwc1 $f4,0(t0)			// Put the sum in f4
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
						: "r" (V2.Values),"r" (Row1), "r" (Row2), "r" (M.Width), "r" (H), "r" (Stride),"r" (W16)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	// The Stride is the number of BYTES between successive rows:
	unsigned MStride=4*4;
	unsigned W=1;
//	const unsigned H=M.Height;
	float *Result=(float *)(V2.Values);
	float *v1=(float *)(V1.Values);  
	//float *v2max=(float *)(&V2.Values[V2.size-1]);
	_asm
	{               
		mov edx,4
		mov edi,V2
		mov esi,M
		Row_Loop:
			mov ebx,dword ptr[v1]
			mov eax,esi
			xorps xmm6,xmm6
			mov ecx,W
			Dot_Product_Loop:
			// Load four values from V2 into xmm0
				movaps xmm1,[eax]
			// Load four values from V1 into xmm2
				movaps xmm2,[ebx]
			// Now xmm2 contains the four values from V1.
			// Multiply the four values from M and V1:
				mulps xmm1,xmm2
//								movups [edx],xmm1
			// Now add the four multiples to the four totals in xmm6:
				subps xmm6,xmm1
//								movups [edx],xmm6
			// Move eax to look at the next four values from this row of M:
				add eax,16
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
			addss xmm6,[edi]
			movss [edi],xmm6
			Add edi,4
			mov ecx,MStride
			add esi,ecx
//								pop edx
			dec edx
			cmp edx,0
		jne Row_Loop
	}
#endif
}
//------------------------------------------------------------------------------
void platform::math::InsertNegativeVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=M.Height||row<0)
		throw Vector3::BadSize();
#endif
#ifdef SIMD
	_asm
	{
		mov edi,M			// Matrix's Values
		mov ecx,[edi]
		mov eax,[edi+8]		// Put W16 into eax
		shl eax,2			// Multiply by 4 to get the matrix stride
		mov edx,row			// Address of row
		mul edx				// Shift in address from the top to the row
		add ecx,eax			// Put the row's address in ecx
		mov ebx,v			// Put Vector's Values in ebx

		xorps xmm1,xmm1
		subps xmm1,[ebx]
		movaps [ecx],xmm1
	}
#else
#ifndef PLAYSTATION2
	unsigned i;
	for(i=0;i<3;i++)
	{
		M.Values[row*M.W16+i]=-v(i);
	}
#else
	asm __volatile__("
		.set noreorder
		add t0,zero,3
		add t2,zero,%1
		lw t1,0(%0)
		add t3,zero,%2	// t3=row
		lw t4,8(%0)		// t4=4
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
						  :  "r" (&M),"r"(v.Values), "r" (row)
						  :);
#endif
#endif
}
//------------------------------------------------------------------------------
void platform::math::AddDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v)
{
	M.Values[0]-=f*(v.Values[2]*v.Values[2]+v.Values[1]*v.Values[1]);
	M.Values[1]+=f*(v.Values[0]*v.Values[1]);
	M.Values[2]+=f*(v.Values[0]*v.Values[2]);

	M.Values[1*4+0]+=f*(v.Values[1]*v.Values[0]);
	M.Values[1*4+1]-=f*(v.Values[2]*v.Values[2]+v.Values[0]*v.Values[0]);
	M.Values[1*4+2]+=f*(v.Values[1]*v.Values[2]);

	M.Values[2*4+0]+=f*(v.Values[2]*v.Values[0]);
	M.Values[2*4+1]+=f*(v.Values[2]*v.Values[1]);
	M.Values[2*4+2]-=f*(v.Values[1]*v.Values[1]+v.Values[0]*v.Values[0]);
}
//------------------------------------------------------------------------------
void platform::math::SubtractDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v)
{
	M.Values[0]+=f*(v.Values[2]*v.Values[2]+v.Values[1]*v.Values[1]);
	M.Values[1]-=f*(v.Values[0]*v.Values[1]);
	M.Values[2]-=f*(v.Values[0]*v.Values[2]);

	M.Values[1*4+0]-=f*(v.Values[1]*v.Values[0]);
	M.Values[1*4+1]+=f*(v.Values[2]*v.Values[2]+v.Values[0]*v.Values[0]);
	M.Values[1*4+2]-=f*(v.Values[1]*v.Values[2]);

	M.Values[2*4+0]-=f*(v.Values[2]*v.Values[0]);
	M.Values[2*4+1]-=f*(v.Values[2]*v.Values[1]);
	M.Values[2*4+2]+=f*(v.Values[1]*v.Values[1]+v.Values[0]*v.Values[0]);
}
//------------------------------------------------------------------------------
void platform::math::AddVectorToRow(Matrix &M,unsigned Row,Vector3 &V)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M.Width;i++)
		M(Row,i)+=V(i);
#else
	float *RowPtr1=&(M.Values[Row*4]);
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
			vadd vf1,vf1,vf2
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
			add.s $f1,$f1,$f2
			swc1 $f1,0(t1)
			addi t1,4				// next four numbers
			addi t0,-1				// counter-=4
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
		mov ecx,M
		mov edi,[ecx]
		mov esi,V
		mov eax,Row
		mul Stride
		add edi,eax
		mov eax,[ecx+16]   //W
		add eax,3
		shr eax,2   
		cmp eax,0
		je iEscape
		iLoop:
			movaps xmm2,[esi]
			addps xmm2,[edi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
		dec eax
		cmp eax,0
		jge iLoop
		iEscape:
	}
#endif
}
//------------------------------------------------------------------------------
void platform::math::AddRow(Vector3 &V1,Matrix &M2,unsigned Row2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M2.Width;i++)
		V1(i)+=M2(Row2,i);
#else
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	asm __volatile__("
		.set noreorder
		add t0,zero,%0					// t0= number of 4-wide columns
		add t1,zero,%1					// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape					// for zero width matrix
		ble t0,3,iLoop2
		nop
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			lqc2 vf2,0(t2)			// Load 4 numbers into vf2
			vadd vf1,vf1,vf2
			sqc2 vf1,0(t1)
			addi t1,16				// next four numbers
			addi t0,-4				// counter-=4
		bgt t0,3,iLoop
			addi t2,16
		blez t0,Escape				// for zero width matrix   
		iLoop2:
			addi t0,-1				// counter-=4
			lwc1 $f1,0(%1)			// Load 4 numbers into vf1
			lwc1 $f2,0(%2)			// Load 4 numbers into vf2
			add.s $f1,$f1,$f2
			swc1 $f1,0(%1)
			addi %1,4				// next four numbers
		bgtz t0,iLoop2
			addi %2,4
		Escape:
	"					:
						: "r" (V1.size), "r" (V1.Values), "r" (RowPtr2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	unsigned Stride2=M2.W16*4;
	_asm
	{
		mov ebx,M2
		mov edi,V1
		mov esi,[ebx]
		mov eax,Row2
		mul Stride2
		add esi,eax
		mov eax,[ebx+16]
		cmp eax,0
		jz iEscape
		iLoop:
			movaps xmm2,[esi]
			addps xmm2,[edi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
			add eax,-4
		cmp eax,0
		jg iLoop
		iEscape:
	}
#endif
}
//------------------------------------------------------------------------------      
void platform::math::RowToVector(const Matrix &M,Vector3 &V,unsigned Row)
{
#ifndef SIMD
#ifndef PLAYSTATION2
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0||3<M.Width)
		throw Vector3::BadSize();
#endif
	for(unsigned i=0;i<M.Width;i++)
	{
		V(i)=M(Row,i);
	}
#else
	float *RowPtr1=&(M.Values[Row*4]);
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
	unsigned W=(M.Width+3);
	_asm
	{
		mov edx,M
		mov edi,[edx]
		mov esi,V
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
float platform::math::MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector3 &V)
{
#ifndef SIMD 
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height||Row<0)
		throw Vector3::BadSize();
#endif
#ifndef PLAYSTATION2
	float sum=0;
	unsigned i;
	for(i=0;i<M.Width;i++)
	{
		sum+=M(Row,i)*V(i);
	}
	return sum;
#else
	float *Row1=&(M.Values[Row*4]);
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
		throw Vector3::BadSize();
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
		mov ebx,V			// Put Vector's Values in ebx
		mov eax,[edi+16]    // Use Width, not W16.
//mov edx,ttt
		xorps xmm0,xmm0
		cmp eax,3
		jle TailLoop
		i_Loop:
			movaps xmm1,[ebx]
			mulps xmm1,[ecx]
			addps xmm0,xmm1
			add ecx,16      // Move along by 4 floats
			add ebx,16		// Move along by 4 floats
			sub eax,4
			cmp eax,3
		jg i_Loop
		cmp eax,0
		jle NoTail
		TailLoop:
			movss xmm1,[ebx]
			mulss xmm1,[ecx]
			addss xmm0,xmm1
			add ecx,4		// Move along by 4 floats
			add ebx,4		// Move along by 4 floats
			sub eax,1
			cmp eax,0
		jg TailLoop      
		mov eax,[edi+16]    // Use Width, not W16.
		cmp eax,4
		jl TailOnly
		NoTail:
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movaps xmm2,xmm0
		shufps xmm2,xmm2,14  // 14 = 2+4+8
		// Add them together, we're only interested in the lower results
		addps xmm0,xmm2
		// Shuffle xmm0's two upper Values into xmm2's two lower slots
		movaps xmm2,xmm0
		shufps xmm2,xmm2,1
		addss xmm0,xmm2
		TailOnly:
		mov eax,t3
		movss [eax],xmm0
	}
	return temp;
#endif
}

void platform::math::InsertVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned col)
{
	for(unsigned i=0;i<3;i++)
		M.Values[i*M.W16+col]=v(i);
}

void platform::math::InsertNegativeVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned col)
{
	for(unsigned i=0;i<3;i++)
		M.Values[i*M.W16+col]=-v(i);
}
void platform::math::TransposeMultiply4(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	V2.Values[0]=M(0,0)*V1.Values[0]+M(1,0)*V1.Values[1]+M(2,0)*V1.Values[2]+M(3,0);
	V2.Values[1]=M(0,1)*V1.Values[0]+M(1,1)*V1.Values[1]+M(2,1)*V1.Values[2]+M(3,1);
	V2.Values[2]=M(0,2)*V1.Values[0]+M(1,2)*V1.Values[1]+M(2,2)*V1.Values[2]+M(3,2);
#else
	asm __volatile__("
		.set noreorder
		lqc2				vf01, 0(%1)			// load vector
		lqc2				vf03, 0(%0)			// load matrix
		lqc2				vf04, 16(%0)
		lqc2				vf05, 32(%0)  
		lqc2				vf06, 48(%0)
		vmulax.xyzw ACC, vf03, vf01	//	apply matrix
		vmadday.xyzw ACC, vf04, vf01     
		vmaddaz.xyzw ACC, vf05, vf01
		vmaddw.xyzw vf02, vf06, vf00	// using w of vf0 so we multiply by 1!
		sqc2 vf02, 0(%2)
	"					  : /* Outputs. */
						  : /* Inputs */ "r" (M.Values), "r" (V1.Values), "r" (V2.Values)
						  : /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");

#endif
#else
	_asm
	{
		mov eax,V1
		movaps xmm1,[eax]
		mov ecx,M
		//mov ecx,[ecx]
		movaps xmm3,[ecx]			// load matrix
		mov ebx,V2
		movaps xmm2,xmm1
		movaps xmm4,[ecx+16]
		shufps xmm2,xmm2,0			// x x x x	= 00 00 00 00
		movaps xmm7,xmm1
		movaps xmm5,[ecx+32]
		shufps xmm7,xmm7,85			// y y y y	= 01 01 01 01
		shufps xmm1,xmm1,170		// z z z z	 = 10 10 10 10
		movaps xmm6,[ecx+48]
		mulps xmm2,xmm3
		mulps xmm7,xmm4
		mulps xmm1,xmm5
		addps xmm2,xmm7
		addps xmm2,xmm1  
		addps xmm2,xmm6 
		movaps [ebx],xmm2
	}
#endif
}

void platform::math::TransposeMultiply3(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	V2.Values[0]=M(0,0)*V1.Values[0]+M(1,0)*V1.Values[1]+M(2,0)*V1.Values[2];
	V2.Values[1]=M(0,1)*V1.Values[0]+M(1,1)*V1.Values[1]+M(2,1)*V1.Values[2];
	V2.Values[2]=M(0,2)*V1.Values[0]+M(1,2)*V1.Values[1]+M(2,2)*V1.Values[2];
#else
	asm __volatile__("
		.set noreorder
		lqc2				vf01, 0(%1)			// load vector
		lqc2				vf03, 0(%0)			// load matrix
		lqc2				vf04, 16(%0)
		lqc2				vf05, 32(%0)
		vmulax.xyzw ACC, vf03, vf01	//	apply matrix
		vmadday.xyzw ACC, vf04, vf01
		vmaddz.xyzw vf02, vf05, vf01
		sqc2 vf02, 0(%2)

	"					  : /* Outputs. */
						  : /* Inputs */ "r" (M.Values), "r" (V1.Values), "r" (V2.Values)
						  : /* Clobber */ "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");

#endif
#else
	_asm
	{
		mov eax,V1
		movaps xmm1,[eax]
		mov ecx,M
		movaps xmm3,[ecx]			// load matrix
		mov ebx,V2
		movaps xmm4,[ecx+16]
		movaps xmm5,[ecx+32]
		movaps xmm2,xmm1
		shufps xmm2,xmm2,192		// x x x w
		movaps xmm6,xmm1
		shufps xmm6,xmm6,213		// y y y w
		shufps xmm1,xmm1,234		// z z z w
		mulps xmm2,xmm3
		mulps xmm6,xmm4
		mulps xmm1,xmm5
		addps xmm2,xmm6
		addps xmm2,xmm1
		movaps [ebx],xmm2
	}
#endif
}
