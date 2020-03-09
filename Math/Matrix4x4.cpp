#define SIM_MATH
#include <string.h>
#include <stdio.h>
#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include "DebugNew.h"			// Enable Visual Studio's memory debugging
#endif

#include <algorithm>
#include <assert.h>

#include "Matrix4x4.h"    
#include "Vector3.h"
#include "SimVector.h"

#include "Platform/Core/RuntimeError.h"


using namespace simul;
using namespace math;

Matrix4x4::Matrix4x4()
{
}     

Matrix4x4::Matrix4x4(const Matrix4x4 &M)
{
	operator=(M);
}

Matrix4x4::Matrix4x4(const float *x)
{
	for(unsigned i=0;i<4;i++)
	{
		for(unsigned j=0;j<4;j++)
			Values[i*4+j]=x[i*4+j];
	}
}

Matrix4x4::Matrix4x4(const double *x)
{
	for(unsigned i=0;i<4;i++)
	{
		for(unsigned j=0;j<4;j++)
			Values[i*4+j]=(float)x[i*4+j];
	}
}

Matrix4x4::Matrix4x4(float x11,float x12,float x13,float x14,
					float x21,float x22,float x23,float x24,
					float x31,float x32,float x33,float x34,
					float x41,float x42,float x43,float x44)
{
	operator()(0,0)=x11;
	operator()(0,1)=x12;
	operator()(0,2)=x13;
	operator()(0,3)=x14;
	operator()(1,0)=x21;
	operator()(1,1)=x22;
	operator()(1,2)=x23;
	operator()(1,3)=x24;
	operator()(2,0)=x31;
	operator()(2,1)=x32;
	operator()(2,2)=x33;
	operator()(2,3)=x34;
	operator()(3,0)=x41;
	operator()(3,1)=x42;
	operator()(3,2)=x43;
	operator()(3,3)=x44;
}
Matrix4x4::~Matrix4x4()
{
//	Values=NULL;
#ifdef CANNOT_ALIGN
//	V=NULL;
#endif
}

float &Matrix4x4::operator()(unsigned row,unsigned col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=4||col>=4||row<0||col<0)
		throw OutOfRange();
#endif           
	return Values[row*4+col];
}  

float Matrix4x4::operator()(unsigned row,unsigned col) const
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=4||col>=4||row<0||col<0)
		throw OutOfRange();
#endif
	return Values[row*4+col];
}

void Matrix4x4::operator=(const Matrix4x4 &M)
{
#ifndef SIMD
	for(unsigned i=0;i<16;i++)
		Values[i]=M.Values[i];
#else
	__asm
	{
		mov edi,this
		mov esi,M
		mov ecx,16

		rep movsd
	}
#endif
}


Matrix4x4 &Matrix4x4::operator*=(float d)
{
	unsigned i,j;
	for (i=0;i<4;i++)
	{
		for (j=0;j<4;j++)
		{
			Values[i*4+j]*=d;
		}
	}
	return *this;
}                       

Matrix4x4 &Matrix4x4::operator/=(float d)
{
	unsigned i,j;
	for (i=0;i<4; i++)
	{
		for (j=0;j<4;j++)
		{
			Values[i*4+j]/=d;
		}
	}
	return *this;
}       
const Vector3 &Matrix4x4::GetRowVector(unsigned r) const
{
	return *(reinterpret_cast<const Vector3 *>(&Values[r*4]));
}
void Matrix4x4::Transpose()
{
	Matrix4x4 T;
	Transpose(T);
	operator=(T);
}
void Matrix4x4::Transpose(Matrix4x4 &T) const
{
#ifdef SIMD
	_asm
	{
		mov eax,this
		movaps xmm0,[eax]	
		movaps xmm1,[eax+16]
		movaps xmm2,[eax+32]
		movaps xmm3,[eax+48]
		movaps xmm4,xmm0
		movaps xmm5,xmm2
		movaps xmm6,xmm0
		movaps xmm7,xmm2
		shufps xmm4,xmm1,0x44
		shufps xmm5,xmm3,0x44
		shufps xmm6,xmm1,0xEE
		shufps xmm7,xmm3,0xEE
		movaps xmm0,xmm4
		movaps xmm1,xmm4
		movaps xmm2,xmm6
		movaps xmm3,xmm6
		shufps xmm0,xmm5,0x88
		shufps xmm1,xmm5,0xDD
		shufps xmm2,xmm7,0x88
		shufps xmm3,xmm7,0xDD
		mov ebx,T
		movaps [ebx]	,xmm0
		movaps [ebx+16]	,xmm1
		movaps [ebx+32]	,xmm2
		movaps [ebx+48]	,xmm3
	};
/*	_MM_TRANSPOSE4_PS(row0, row1, row2, row3)
            tmp0   = _mm_shuffle_ps((row0), (row1), 0x44);          \
            tmp2   = _mm_shuffle_ps((row0), (row1), 0xEE);          \
            tmp1   = _mm_shuffle_ps((row2), (row3), 0x44);          \
            tmp3   = _mm_shuffle_ps((row2), (row3), 0xEE);          \
                                                                    \
            (row0) = _mm_shuffle_ps(tmp0, tmp1, 0x88);              \
            (row1) = _mm_shuffle_ps(tmp0, tmp1, 0xDD);              \
            (row2) = _mm_shuffle_ps(tmp2, tmp3, 0x88);              \
            (row3) = _mm_shuffle_ps(tmp2, tmp3, 0xDD);              \*/
#else
	for(unsigned i=0;i<4;i++)
	{
		for(unsigned j=0;j<4;j++)
			T.Values[i*4+j]=Values[j*4+i];
	}
#endif
}

void Matrix4x4::Zero()
{
#ifndef SIMD
	for(unsigned i=0;i<16;i++)
			Values[i]=0;
#else
	unsigned Stride=4*4;
	_asm
	{
		mov edi,this
		mov ecx,16
		xor eax,eax
		//mov edi,[ebx]
		rep stos eax
	}
#endif
}

void Matrix4x4::ResetToUnitMatrix()
{
	Zero();
	for(unsigned i=0;i<4;i++)
		Values[i*5]=1.f;
}

Matrix4x4 Matrix4x4::IdentityMatrix()
{
	 Matrix4x4 I;
	 I.ResetToUnitMatrix();
	 return I;
}

void Matrix4x4::Set(const float x[])
{
	for(unsigned i=0;i<16;i++)
		Values[i]=x[i];
}

void Matrix4x4::AddScalarTimesMatrix(float val,const Matrix4x4 &M2)
{
	unsigned i,j;
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			Values[i*4+j]+=val*M2.Values[i*4+j];
		}
	}
}
#include "Quaternion.h"
namespace simul
{
	namespace math
	{
		void AddFloatTimesMatrix4x4(Matrix4x4 &result,const float f,const Matrix4x4 &M)
		{
			unsigned i,j;
			for(i=0;i<4;i++)
			{
				for(j=0;j<4;j++)
				{
					result.Values[i*4+j]+=f*M.Values[i*4+j];
				}
			}
		}
	
		Matrix4x4 SIMUL_MATH_EXPORT_FN Rotate(const Matrix4x4 &A,float angle,const Vector3 &axis)
		{
			Quaternion q(angle,axis);
			Matrix4x4 m(q);
			Matrix4x4 M;
			Multiply4x4(M,m,A);
			return M;
		}

		void Multiply4x4(Matrix4x4 &M,const Matrix4x4 &A,const Matrix4x4 &B)
		{
			for(int i=0;i<4;i++)
			{
				for(int j=0;j<4;j++)
				{
					const float *m1row=&A.Values[i*4+0];
					float t=0.f;
					int k=0;
					t+=m1row[k]*B(k,j);	++k;
					t+=m1row[k]*B(k,j);	++k;
					t+=m1row[k]*B(k,j);	++k;
					t+=m1row[k]*B(k,j);
					M(i,j)=t;
				}
			}
		}

		void Multiply4x4ByTranspose(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2)
		{
			unsigned i,j;
			for(i=0;i<4;i++)
			{
				for(j=0;j<4;j++)
				{
					float t=0.f;
					unsigned k=0;
					t+=M1.Values[i*4+k]*M2(j,k);	++k;
					t+=M1.Values[i*4+k]*M2(j,k);	++k;
					t+=M1.Values[i*4+k]*M2(j,k);
					M.Values[i*4+j]=t;
				}
			}
		}
		void Multiply3x3(Matrix4x4 &M,const Matrix4x4 &A,const Matrix4x4 &B)
		{
		#ifdef PLAYSTATION2
			unsigned BStride=4*4;			
			unsigned MStride=4*4;
			unsigned AStride=4*4;
			unsigned HW=3;
			//unsigned BWidth=(B.Width+3)&(~0x3);
			asm __volatile__
			(
			"   
			.set noreorder
				xor t5,t5			// j=0
				blez %8,iEscape
				iLoop:
					blez %7,jEscape
					xor t8,t8
					add t4,zero,%7	// j=BWidth
					jLoop:
						lw t0,0(%0)	// M0
						lw t1,0(%1)	// A0
						lw t2,0(%2)	// B0
						mul t3,t5,%3
						add t0,t3
						add t0,t8
						mul t3,t5,%4
						add t1,t3
						add t2,t8
						blez %6,kEscape  
						add t9,zero,%6
						vsub vf10,vf00,vf00
						ble t9,3,kTail
						kLoop:
							lqc2 vf01,0(t1)			// APos
							lqc2 vf02,0(t2)			// BPos
							add t2,%5
							lqc2 vf03,0(t2)			// BPos
							add t2,%5
							lqc2 vf04,0(t2)			// BPos
							add t2,%5
							lqc2 vf05,0(t2)			// BPos
							add t2,%5
							vmulax.xyzw		ACC, vf02, vf01
							vmadday.xyzw	ACC, vf03, vf01	
							vmaddaz.xyzw	ACC, vf04, vf01	
							vmaddw.xyzw		vf01,vf05, vf01	
							vadd vf10,vf10,vf01
							addi t9,-4
						bgt t9,3,kLoop
							addi t1,16
						blez t9,kFinish		// If Width1 or height of 2 is a multiple of 4.
						kTail:
						beq t9,2,Tail2
						beq t9,3,Tail3
						nop
						Tail1:
						lqc2 vf01,0(t1)			
						lqc2 vf02,0(t2)			
						vmulx vf1,vf2,vf1x
						vadd vf10,vf10,vf1
						sqc2 vf10,0(t0)
						b kFinish
	
						Tail2:
						lqc2 vf01,0(t1)			
						lqc2 vf02,0(t2)			
						add t2,%5
						lqc2 vf03,0(t2)			
						vmulax ACC,vf2,vf1x
						vmaddy vf1,vf3,vf1y
						vadd vf10,vf10,vf1
						b kFinish
	
						Tail3:
						lqc2 vf01,0(t1)			// APos
						lqc2 vf02,0(t2)			// BPos	
						add t2,%5
						lqc2 vf03,0(t2)			
						add t2,%5
						lqc2 vf04,0(t2)		
						add t2,%5
						vmulax ACC,vf2,vf1
						vmadday	ACC,vf3,vf1	
						vmaddz vf1,vf4,vf1
						vadd vf10,vf10,vf1

						kFinish:
						sqc2 vf10,0(t0)
						kEscape:
						addi t4,-4				// j+=4
					bgtz t4,jLoop				// >0
						addi t8,16
					jEscape:
					addi t5,1				// i+=1
					sub t3,%8,t5			// AHeight-i
				bgtz t3,iLoop				// >0
				nop
				iEscape:
					.set reorder
					"	:  
						: "g"(&M),"g"(&A),"g"(&B), "g"(MStride),"g"(AStride),
							"g"(BStride),"g"(HW),"g"(HW),"g"(HW)
						:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
							"$vf1", "$vf2", "$vf3", "$vf4", "$vf5","$vf10"
					);
		//    M.CheckEdges();
		#else
			unsigned i,j,k;
			for(i=0;i<3;i++)
			{
				for(j=0;j<3;j++)
				{
					const float *m1row=&A.Values[i*4+0];
					float t=0.f;
					k=0;
					t+=m1row[k]*B(k,j);	++k;
					t+=m1row[k]*B(k,j);	++k;
					t+=m1row[k]*B(k,j);
					M(i,j)=t;
				}
			}
		//    M.CheckEdges();
		#endif
		}                

		void Multiply3x3ByTranspose(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2)
		{
			unsigned i,j;
			for(i=0;i<3;i++)
			{
				for(j=0;j<3;j++)
				{
					float t=0.f;
					unsigned k=0;
					t+=M1.Values[i+k*4]*M2(j,k);	++k;
					t+=M1.Values[i+k*4]*M2(j,k);	++k;
					t+=M1.Values[i+k*4]*M2(j,k);
					M.Values[i*4+j]=t;
				}
			}
		}

		void MultiplyTransposeByMatrix3x3(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2)
		{
			unsigned i,j;
			for(i=0;i<3;i++)
			{
				for(j=0;j<3;j++)
				{
					float t=0.f;
					unsigned k=0;
					t+=M1.Values[k*4+i]*M2(k,j);	++k;
					t+=M1.Values[k*4+i]*M2(k,j);	++k;
					t+=M1.Values[k*4+i]*M2(k,j);
					M.Values[i*4+j]=t;
				}
			}
		}
	}
}

void TransposeMultiply3x3(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1)
{
#ifdef SIMD
	// The Stride is the number of BYTES between successive rows:        
	unsigned MStride=4*4;
	unsigned W=(3+3)>>2;
	const unsigned H=3;
	//const float *MPtr=M.Values;
	//float *v1=(float *)(V1.Values);
	_asm
	{
		mov edx,H    
		mov edi,V2
		mov esi,M
		Row_Loop:  
			mov ebx,V1
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
	for(i=0;i<3;i++)
	{
		V2.Values[i]=0.f;
		for(j=0;j<3;j++)
			V2.Values[i]+=M(j,i)*V1.Values[j];
	}
#endif
}   

bool Matrix4x4::SymmetricInverse3x3(Matrix4x4 &Result)
{
	Matrix4x4 Fac;
	if(!CholeskyFactorization(Fac))
		return false;
	InverseFromCholeskyFactors(Result,Fac);
	return true;
}

bool Matrix4x4::CholeskyFactorization(Matrix4x4 &Fac) const
{
	float sum;
	for(int i=0;i<4;i++)
	{
		for(int j=i;j<4;j++)
		{
			sum=operator()(j,i);
			for(int k=0;k<i;k++)
				sum-=Fac(i,k)*Fac(j,k);
			if(i==j)
            {
				if(sum<0)
                {
                    return false;
                }
		   		sum=sqrt(sum);
			}
			else
			   	sum/=Fac(i,i);
			Fac(j,i)=sum;
		}
	}
	return true;
}
void Matrix4x4::Inverse(Matrix4x4 &Inv) const
{
	
	float *inv=Inv.RowPointer(0);//[16];
	float det;
    int i;

    inv[0] = Values[5]  * Values[10] * Values[15] - 
             Values[5]  * Values[11] * Values[14] - 
             Values[9]  * Values[6]  * Values[15] + 
             Values[9]  * Values[7]  * Values[14] +
             Values[13] * Values[6]  * Values[11] - 
             Values[13] * Values[7]  * Values[10];

    inv[4] = -Values[4]  * Values[10] * Values[15] + 
              Values[4]  * Values[11] * Values[14] + 
              Values[8]  * Values[6]  * Values[15] - 
              Values[8]  * Values[7]  * Values[14] - 
              Values[12] * Values[6]  * Values[11] + 
              Values[12] * Values[7]  * Values[10];

    inv[8] = Values[4]  * Values[9] * Values[15] - 
             Values[4]  * Values[11] * Values[13] - 
             Values[8]  * Values[5] * Values[15] + 
             Values[8]  * Values[7] * Values[13] + 
             Values[12] * Values[5] * Values[11] - 
             Values[12] * Values[7] * Values[9];

    inv[12] = -Values[4]  * Values[9] * Values[14] + 
               Values[4]  * Values[10] * Values[13] +
               Values[8]  * Values[5] * Values[14] - 
               Values[8]  * Values[6] * Values[13] - 
               Values[12] * Values[5] * Values[10] + 
               Values[12] * Values[6] * Values[9];

    inv[1] = -Values[1]  * Values[10] * Values[15] + 
              Values[1]  * Values[11] * Values[14] + 
              Values[9]  * Values[2] * Values[15] - 
              Values[9]  * Values[3] * Values[14] - 
              Values[13] * Values[2] * Values[11] + 
              Values[13] * Values[3] * Values[10];

    inv[5] = Values[0]  * Values[10] * Values[15] - 
             Values[0]  * Values[11] * Values[14] - 
             Values[8]  * Values[2] * Values[15] + 
             Values[8]  * Values[3] * Values[14] + 
             Values[12] * Values[2] * Values[11] - 
             Values[12] * Values[3] * Values[10];

    inv[9] = -Values[0]  * Values[9] * Values[15] + 
              Values[0]  * Values[11] * Values[13] + 
              Values[8]  * Values[1] * Values[15] - 
              Values[8]  * Values[3] * Values[13] - 
              Values[12] * Values[1] * Values[11] + 
              Values[12] * Values[3] * Values[9];

    inv[13] = Values[0]  * Values[9] * Values[14] - 
              Values[0]  * Values[10] * Values[13] - 
              Values[8]  * Values[1] * Values[14] + 
              Values[8]  * Values[2] * Values[13] + 
              Values[12] * Values[1] * Values[10] - 
              Values[12] * Values[2] * Values[9];

    inv[2] = Values[1]  * Values[6] * Values[15] - 
             Values[1]  * Values[7] * Values[14] - 
             Values[5]  * Values[2] * Values[15] + 
             Values[5]  * Values[3] * Values[14] + 
             Values[13] * Values[2] * Values[7] - 
             Values[13] * Values[3] * Values[6];

    inv[6] = -Values[0]  * Values[6] * Values[15] + 
              Values[0]  * Values[7] * Values[14] + 
              Values[4]  * Values[2] * Values[15] - 
              Values[4]  * Values[3] * Values[14] - 
              Values[12] * Values[2] * Values[7] + 
              Values[12] * Values[3] * Values[6];

    inv[10] = Values[0]  * Values[5] * Values[15] - 
              Values[0]  * Values[7] * Values[13] - 
              Values[4]  * Values[1] * Values[15] + 
              Values[4]  * Values[3] * Values[13] + 
              Values[12] * Values[1] * Values[7] - 
              Values[12] * Values[3] * Values[5];

    inv[14] = -Values[0]  * Values[5] * Values[14] + 
               Values[0]  * Values[6] * Values[13] + 
               Values[4]  * Values[1] * Values[14] - 
               Values[4]  * Values[2] * Values[13] - 
               Values[12] * Values[1] * Values[6] + 
               Values[12] * Values[2] * Values[5];

    inv[3] = -Values[1] * Values[6] * Values[11] + 
              Values[1] * Values[7] * Values[10] + 
              Values[5] * Values[2] * Values[11] - 
              Values[5] * Values[3] * Values[10] - 
              Values[9] * Values[2] * Values[7] + 
              Values[9] * Values[3] * Values[6];

    inv[7] = Values[0] * Values[6] * Values[11] - 
             Values[0] * Values[7] * Values[10] - 
             Values[4] * Values[2] * Values[11] + 
             Values[4] * Values[3] * Values[10] + 
             Values[8] * Values[2] * Values[7] - 
             Values[8] * Values[3] * Values[6];

    inv[11] = -Values[0] * Values[5] * Values[11] + 
               Values[0] * Values[7] * Values[9] + 
               Values[4] * Values[1] * Values[11] - 
               Values[4] * Values[3] * Values[9] - 
               Values[8] * Values[1] * Values[7] + 
               Values[8] * Values[3] * Values[5];

    inv[15] = Values[0] * Values[5] * Values[10] - 
              Values[0] * Values[6] * Values[9] - 
              Values[4] * Values[1] * Values[10] + 
              Values[4] * Values[2] * Values[9] + 
              Values[8] * Values[1] * Values[6] - 
              Values[8] * Values[2] * Values[5];

    det = Values[0] * inv[0] + Values[1] * inv[4] + Values[2] * inv[8] + Values[3] * inv[12];

    if (det == 0)
	{
		SIMUL_THROW("Matrix has zero determinant - can't invert.");
        return;
	}

    det = 1.f / det;

    for (i = 0; i < 16; i++)
        inv[i] = inv[i] * det;
}

// The matrix for which Mt * v transforms from normalized texture coordinates into
// world position, where z=0 is cloudbase and z=1 is the top of the clouds.
Matrix4x4 Matrix4x4::ObliqueToCartesianTransform(int a,Vector3 a_dir,Vector3 a1_dir,Vector3 origin,Vector3 scales)
{
	Matrix4x4 trans;
	trans.Zero();
	int a0=(a+1)%3;
	int a1=(a+2)%3;
	Vector3 a0_dir;
	CrossProduct(a0_dir,a1_dir,Vector3(0,0,1.f));
	a0_dir.Normalize();
	// Make the a1 axis perpendicular to 
	// make it 1 on the a, in the direction light is shining:
	a_dir			=a_dir/fabs(a_dir(a));
	trans(a0,a0)	=a0_dir(a0)*scales(a0);
	trans(a0,a1)	=a1_dir(a0)*scales(a1);
	trans(a0,a)		=a_dir(a0)*scales(a);
	trans(a0,3)		=origin(a0);
	trans(a1,a0)	=a0_dir(a1)*scales(a0);
	trans(a1,a1)	=a1_dir(a1)*scales(a1);
	trans(a1,a)		=a_dir(a1)*scales(a);
	trans(a1,3)		=origin(a1);
	trans(a,a0)		=0;
	trans(a,a1)		=0;
	trans(a,a)		=scales(a);
	trans(a,3)		=origin(a);
	trans(3,3)		=1.f;
	return trans;
}

Matrix4x4 Matrix4x4::DefineFromYZ(Vector3 y_dir,Vector3 z_dir)
{
	Matrix4x4 T4;
	T4.ResetToUnitMatrix();
	Vector3 x_dir;
	Vector3 y_ortho;
	Vector3 z_ortho;
	CrossProduct(x_dir,y_dir,z_dir);
	CrossProduct(y_ortho,z_dir,x_dir);
	CrossProduct(z_ortho,x_dir,y_ortho);
	T4.InsertRow(0,x_dir);
	T4.InsertRow(1,y_ortho);
	T4.InsertRow(2,z_ortho);
	return T4;
}

void Matrix4x4::SwapYAndZ(Matrix4x4 &dest)
{
	std::swap(dest._21,dest._31);
	std::swap(dest._13,dest._12);
	std::swap(dest._24,dest._34);
	std::swap(dest._42,dest._43);
	std::swap(dest._23,dest._32);
	std::swap(dest._22,dest._33);
}

void Matrix4x4::ReverseY(Matrix4x4 &dest)
{
	dest._21*=-1.f;
	dest._23*=-1.f;
	dest._24*=-1.f;

	dest._12*=-1.f;
	dest._32*=-1.f;
	dest._42*=-1.f;
}

Matrix4x4 Matrix4x4::RotationX(float r)
{
	Matrix4x4 R(1.f		,0		,0		,0
				,0.f	,cos(r)	,-sin(r),0
				,0.f	,sin(r)	,cos(r)	,0
				,0.f	,0.f	,0.f	,1.f);
	return R;
}

Matrix4x4 Matrix4x4::RotationY(float r)
{
	Matrix4x4 R(cos(r)	,0		,sin(r)	,0
				,0		,1.0f	,0		,0
				,-sin(r),0		,cos(r)	,0
				,0		,0		,0		,1.0f);
	return R;
}

Matrix4x4 Matrix4x4::RotationZ(float r)
{
	Matrix4x4 R(cos(r)	,-sin(r),0		,0
				,sin(r)	,cos(r)	,0		,0
				,0		,0		,1.f	,0
				,0		,0		,0		,1.f);
	return R;
}

Matrix4x4 Matrix4x4::Translation(float x,float y,float z)
{
	Matrix4x4 R(1.0f	,0.0f	,0.0f	,0.0f
				,0.0f	,1.0f	,0.0f	,0.0f
				,0.0f	,0.0f	,1.f	,0.0f
				,x		,y		,z		,1.f);
	return R;
}

Matrix4x4 Matrix4x4::Translation(const float *x)
{
	return Translation(x[0],x[1],x[2]);
}

void Matrix4x4::SimpleInverse(Matrix4x4 &Inv) const
{
	const Vector3 *XX=reinterpret_cast<const Vector3*>(RowPointer(0));
	const Vector3 *YY=reinterpret_cast<const Vector3*>(RowPointer(1));
	const Vector3 *ZZ=reinterpret_cast<const Vector3*>(RowPointer(2));
	Transpose(Inv);
	const Vector3 &xe=*(reinterpret_cast<const Vector3*>(RowPointer(3)));

	Inv(0,3)=0;
	Inv(1,3)=0;
	Inv(2,3)=0;
	Inv(3,0)=-((xe)*(*XX));
	Inv(3,1)=-((xe)*(*YY));
	Inv(3,2)=-((xe)*(*ZZ));
	Inv(3,3)=1.f;
}

void Matrix4x4::InverseFromCholeskyFactors(Matrix4x4 &Result,Matrix4x4 &Fac) const
{
	unsigned i,j,k;
// Cholesky:
	float sum;
//Invert:
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			sum=0;
			if(i==j)
				sum=1.f;
			for(k=0;k<j;k++)
				sum-=Fac(j,k)*Result(i,k);
            sum/=Fac(j,j);
			Result(i,j)=sum;
        }
		for(j=3-1;j>=i;j--)
		{
			sum=Result(i,j);
			for(k=j+1;k<3;k++)
				sum-=Fac(k,j)*Result(i,k);
			sum/=Fac(j,j);
			Result(i,j)=sum;
		}
	}
	for(i=0;i<4;i++)
	{
		for(j=i+1;j<4;j++)
		{
			Result(j,i)=Result(i,j);
		}
	}
}            
Matrix4x4 Temp3x3;

void Matrix4x4::operator+=(const Matrix4x4 &M)
{
#ifndef SIMD
unsigned i,j;
	for (i=0;i<4; i++)
	{
		for (j=0;j<4;j++)
		{
			Values[i*4+j]+=M.Values[i*4+j];
		}
	}
#else
	unsigned MStride=4*4;
	unsigned Stride=4*4;
	_asm
	{
		mov ecx,this
		mov ebx,M
		mov esi,this
		mov edi,M
		mov eax,4
		Row_Loop:      
			mov ebx,4
			push esi
			push edi
			Col_Loop:
				movaps xmm2,[edi]  
				movaps xmm1,[esi]
				addps xmm1,xmm2
				movaps [esi],xmm1
				add edi,16
				add esi,16
				sub ebx,4   
				cmp ebx,0
			jg Col_Loop
			pop edi    
			pop esi
			add edi,MStride
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop
	}
#endif
}

void Matrix4x4::operator-=(const Matrix4x4 &M)
{
#ifndef SIMD
	unsigned i,j;
	for(i=0;i<4;i++)
	{
		for (j=0;j<4;j++)
		{
			Values[i*4+j]=Values[i*4+j]-M.Values[i*4+j];
		}
	}
#else
	unsigned MStride=4*4;
	unsigned Stride=4*4;
	float *val=Values;
	const float *Mval=M.Values;
	if(!Mval)
		return;
	unsigned H1=4;
	unsigned W1=4;
	_asm
	{                     
		mov esi,this
		mov edi,M
		mov eax,H1
		Row_Loop:
			mov ebx,W1
			push esi
			push edi
			Col_Loop:
				movaps xmm1,[edi]  
				movaps xmm2,[esi]
				subps xmm2,xmm1
				movaps [esi],xmm2
				add edi,16
				add esi,16
				sub ebx,4   
				cmp ebx,0
			jg Col_Loop
			pop edi    
			pop esi
			add edi,MStride
			add esi,Stride
			dec eax   
			cmp eax,0
		jg Row_Loop
	}
#endif
}
void Matrix4x4::InsertColumn(int col,Vector3 v)
{
	assert(col>=0&&col<4);
	Values[col  ]=v.x;
	Values[col+4]=v.y;
	Values[col+8]=v.z;
}
void Matrix4x4::InsertRow(int r,Vector3 v)
{
	assert(r>=0&&r<4);
	Values[r*4+0]=v.x;
	Values[r*4+1]=v.y;
	Values[r*4+2]=v.z;
}

void Matrix4x4::ScaleRows(const float s[4])
{
	for(int r=0;r<4;r++)
	{
		for(int c=0;c<4;c++)
			operator()(r,c)*=s[r];
	}
}

void Matrix4x4::ScaleColumns(const float s[4])
{
	for(int c=0;c<4;c++)
	{
		for(int r=0;r<4;r++)
			operator()(r,c)*=s[c];
	}
}

void simul::math::MultiplyAndSubtract(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2)
{
	unsigned i,j,k;
	for(i=0;i<4;++i)
	{
		for(j=0;j<4;j++)
		{
			for(k=0;k<4;k++)
			{
				M.Values[i*4+j]-=M1(i,k)*M2(k,j);
			}
		}
	}
}

void simul::math::MultiplyByScalar(Matrix4x4 &result,const float f,const Matrix4x4 &M)
{
	unsigned i,j;
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			result.Values[i*4+j]=f*M.Values[i*4+j];
		}
	}
}
