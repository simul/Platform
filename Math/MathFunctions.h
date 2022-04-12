#ifndef MathFunctionsH
#define MathFunctionsH
#ifdef _MSC_VER
	
	#pragma warning(push)
#endif


#include <stdlib.h>

#ifdef PLAYSTATION_2
#include <math.h>
#endif

#include <float.h>
#include "Export.h"
#include "Decay.h"

#ifdef PLAYSTATION_2
	#define Fabs fabsf
#else
#include <math.h>
	#define Fabs fabs
#endif

#ifdef _MSC_VER
	#define ALIGN32  __declspec(align(32))
#else
	#define ALIGN32
#endif

#if defined(_MSC_VER) && defined(INTEL_COMPATIBLE) && !defined(__GNUC__) && !defined(WIN64)
	#define USE_ASSEMBLY_COMPARE
#endif

#ifdef _MSC_VER
#pragma warning( disable : 4324 4258 4288 4289 4715 4716)
#pragma warning( disable : 4715 4716)
#pragma warning( disable : 4716)
#endif

namespace platform
{           
	namespace math
	{
		extern float SIMUL_MATH_EXPORT_FN InverseTangent(float y,float x);
		inline bool CompareNormalList(const float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,int Num)
		{
		#ifdef USE_ASSEMBLY_COMPARE
			x;NormalX;NormalY;NormalZ;NormalD;Num;
			_asm
			{
				mov ebx,Num
				mov ecx,x
				movss xmm4,[ecx]
				movss xmm5,[ecx+4]
				movss xmm6,[ecx+8]
				shufps xmm4,xmm4,0
				shufps xmm5,xmm5,0
				shufps xmm6,xmm6,0
				mov ecx,NormalX
				mov esi,NormalY
				mov edi,NormalZ
				mov edx,NormalD
				iLoop:
					movaps xmm1,[ecx]
					movaps xmm2,[esi]
					movaps xmm3,[edi]
					movaps xmm7,[edx]
					mulps xmm1,xmm4
					mulps xmm2,xmm5
					mulps xmm3,xmm6
					addps xmm1,xmm2
					addps xmm1,xmm3
					add ecx,16
					add edi,16
					add edx,16
					cmpltps xmm7,xmm1
					add ebx,-4
					add esi,16
					movmskps eax,xmm7
					cmp eax,0
					jne retfalse
					cmp ebx,0
				jg iLoop
				mov al,1
				jmp rettrue
				retfalse:
				mov al,0
				rettrue:
			};
		#else
			for(int i=0;i<Num;i+=4)
			{
				ALIGN32 float d1[4];
				ALIGN32 float d2[4];
				ALIGN32 float d3[4];
				for(int j=0;j<4;j++)
				{
					d1[j]=NormalX[i+j]*x[0];
					d2[j]=NormalY[i+j]*x[1];
					d3[j]=NormalZ[i+j]*x[2];
				}
				for(int j=0;j<4;j++)
					d1[j]+=d2[j]+d3[j];
				for(int j=0;j<4;j++)
					if(d1[j]>NormalD[i+j])
						return false;
			}
			return true;
		#endif      
		}
		inline bool LocalOnSurfaceNormalList(float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,int Num,int except_surface)
		{
		#ifdef USE_ASSEMBLY_COMPARE
			static float big_number=1e12f;
			_asm
			{
				mov ebx,Num
				mov ecx,x
				movss xmm4,[ecx]
				movss xmm5,[ecx+4]
				movss xmm6,[ecx+8]
				mov edx,NormalD
				mov ecx,except_surface		// don't test this surface. NormalD[except_surface] is at addr. NormalD+4*except_surface
				lea eax,[edx+4*ecx]
				mov ecx,[eax]				// contents of NormalD[except_surface]
				push ecx					// store it
				push eax					// store it
				mov ecx,big_number
				mov [eax],ecx				// put big number in NormalD[except_surface]
				shufps xmm4,xmm4,0
				shufps xmm5,xmm5,0
				shufps xmm6,xmm6,0
				mov ecx,NormalX
				mov esi,NormalY
				mov edi,NormalZ
				iLoop:
					movaps xmm1,[ecx]
					movaps xmm2,[esi]
					movaps xmm3,[edi]
					movaps xmm7,[edx]
					mulps xmm1,xmm4
					mulps xmm2,xmm5
					mulps xmm3,xmm6
					addps xmm1,xmm2
					addps xmm1,xmm3
					add ecx,16
					add edi,16
					add edx,16
					cmpltps xmm7,xmm1
					add ebx,-4
					add esi,16
					movmskps eax,xmm7
					cmp eax,0
					jne retfalse
					cmp ebx,0
				jg iLoop
				mov al,1
				jmp rettrue
				retfalse:
				mov al,0
				rettrue:
				pop edx
				pop ecx
				mov [edx],ecx
			};
		#else
			for(int i=0;i<Num;i+=4)
			{
				ALIGN32 float d1[4];
				ALIGN32 float d2[4];
				ALIGN32 float d3[4];
				for(int j=0;j<4;j++)
				{
					d1[j]=NormalX[i+j]*x[0];
					d2[j]=NormalY[i+j]*x[1];
					d3[j]=NormalZ[i+j]*x[2];
				}
				for(int j=0;j<4;j++)
					d1[j]+=d2[j]+d3[j];
				for(int j=0;j<4;j++)
					if(i+j!=except_surface)
						if(d1[j]>NormalD[i+j])
							return false;
			}
			return true;
		#endif      
		}
		inline bool LocalOnSurfaceNormalList(float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,int Num,float t,int except_surface)
		{
		#ifdef USE_ASSEMBLY_COMPARE
			static float big_number=1e12f;
			_asm
			{
				movss xmm0,t
				shufps xmm0,xmm0,0
				mov ebx,Num
				mov ecx,x
				movss xmm4,[ecx]
				movss xmm5,[ecx+4]
				movss xmm6,[ecx+8]
				mov edx,NormalD
				mov ecx,except_surface		// don't test this surface. NormalD[except_surface] is at addr. NormalD+4*except_surface
				lea eax,[edx+4*ecx]
				mov ecx,[eax]				// contents of NormalD[except_surface]
				push ecx					// store it
				push eax					// store it
				mov ecx,big_number
				mov [eax],ecx				// put big number in NormalD[except_surface]
				shufps xmm4,xmm4,0
				shufps xmm5,xmm5,0
				shufps xmm6,xmm6,0
				mov ecx,NormalX
				mov esi,NormalY
				mov edi,NormalZ
				iLoop:
					movaps xmm1,[ecx]
					movaps xmm2,[esi]
					movaps xmm3,[edi]
					movaps xmm7,[edx]
					addps xmm7,xmm0
					mulps xmm1,xmm4
					mulps xmm2,xmm5
					mulps xmm3,xmm6
					addps xmm1,xmm2
					addps xmm1,xmm3
					add ecx,16
					add edi,16
					add edx,16
					cmpltps xmm7,xmm1
					add ebx,-4
					add esi,16
					movmskps eax,xmm7
					cmp eax,0
					jne retfalse
					cmp ebx,0
				jg iLoop
				mov al,1
				jmp rettrue
				retfalse:
				mov al,0
				rettrue:
				pop edx
				pop ecx
				mov [edx],ecx
			};
		#else
			for(int i=0;i<Num;i+=4)
			{
				ALIGN32 float d1[4];
				ALIGN32 float d2[4];
				ALIGN32 float d3[4];
				for(int j=0;j<4;j++)
				{
					d1[j]=NormalX[i+j]*x[0];
					d2[j]=NormalY[i+j]*x[1];
					d3[j]=NormalZ[i+j]*x[2];
				}
				for(int j=0;j<4;j++)
					d1[j]+=d2[j]+d3[j];
				for(int j=0;j<4;j++)
					if(i+j!=except_surface)
						if(d1[j]>NormalD[i+j]+t)
							return false;
			}
			return true;
		#endif      
		}
		extern bool CompareNormalList(float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,int Num,float t);
		inline bool CompareNormalList2(float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,int Num)
		{
			for(int i=0;i<Num;i+=4)
			{     
				ALIGN32 float d1[4];
				ALIGN32 float d2[4];
				ALIGN32 float d3[4];
				for(int j=0;j<4;j++)
				{
					d1[j]=NormalX[i+j]*x[0];
					d2[j]=NormalY[i+j]*x[1];
					d3[j]=NormalZ[i+j]*x[2];
				}
				for(int j=0;j<4;j++)
					d1[j]+=d2[j]+d3[j];
				for(int j=0;j<4;j++)
					if(d1[j]>NormalD[i+j])
						return false;
			}
			return true;
		}
		inline bool ComparePositionListToSurface(float *PointsX,float *PointsY,float *PointsZ,float *N,float D,int Num)
		{
		#ifdef USE_ASSEMBLY_COMPARE
			_asm
			{
				mov ebx,Num
				cmp ebx,0
				je return_true
				mov ecx,N
				movss xmm4,[ecx]
				movss xmm5,[ecx+4]
				movss xmm6,[ecx+8]
				shufps xmm4,xmm4,0
				shufps xmm5,xmm5,0
				shufps xmm6,xmm6,0
				mov ecx,PointsX
				mov esi,PointsY
				mov edi,PointsZ
				movss xmm7,D
				prefetcht2 [ecx+16]
				shufps xmm7,xmm7,0
				iLoop:
					movaps xmm1,[ecx]
					movaps xmm2,[esi]
					movaps xmm3,[edi]
					add ecx,16
					add edi,16
					mulps xmm1,xmm4
					mulps xmm2,xmm5
					mulps xmm3,xmm6
					addps xmm1,xmm2
					addps xmm1,xmm3
					cmpltps xmm1,xmm7
					add ebx,-4
					add esi,16
					movmskps eax,xmm1
					cmp eax,0
					jne return_false
					cmp ebx,0
				jg iLoop
				mov al,1
				jmp return_true
			return_false:
				mov al,0
			return_true:
			};
		#else
			N[3]=D;
			for(int i=0;i<Num;i+=4)
			{     
				ALIGN32 float d1[4];
				ALIGN32 float d2[4];
				ALIGN32 float d3[4];
				for(int j=0;j<4;j++)
				{
					d1[j]=PointsX[i+j]*N[0];
					d2[j]=PointsY[i+j]*N[1];
					d3[j]=PointsZ[i+j]*N[2];
				}
				for(int j=0;j<4;j++)
					d1[j]+=d2[j]+d3[j];
				for(int j=0;j<4;j++)
					if(d1[j]<D)
						return false;
			}
			return true;
		#endif
		}
		extern void PowerOfTen(float num,float &exp);
		extern void IntegerPowerOfTen(float num,float &man,int &exp);
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif
