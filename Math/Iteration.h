#ifndef IterationH
#define IterationH

#include <vector>
#include "Platform/Math/Matrix.h"
#include "Platform/Math/SimVector.h"
#include "Platform/Math/Export.h"

#ifdef _MSC_VER
	#define ALIGN32  __declspec(align(32))
#else
	#define ALIGN32
#endif

namespace simul
{
	namespace math
	{
extern void SIMUL_MATH_EXPORT_FN SSORP(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals);       
extern void SIMUL_MATH_EXPORT_FN SSORP2(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals,void* Nv);

#ifdef SIMD
inline void AddDotProduct8(float &f,float *V1,float *V2)
{
	float temp=f;
	_asm 
	{
		mov eax,V1
		prefetcht1 [eax+32]
		movaps xmm1,[eax]
		mov ebx,V2
		mulps xmm1,[ebx]
		xorps xmm2,xmm2
		movss xmm2,temp;
		addps xmm2,xmm1
		movaps xmm1,[eax+16]
		mulps xmm1,[ebx+16]
		addps xmm2,xmm1
		movhlps xmm1,xmm2
		addps xmm2,xmm1
		pshufd xmm3,xmm2,01010101b
		addss xmm2,xmm3
		movss temp,xmm2
	}
	f=temp;
}
#else
extern void AddDotProduct8(float &f,float *V1,float *V2);
#endif
// = X+Z,Y+W,--,--	
// Y+W,--,--,--
           
#ifdef SIMD
inline void AddFloatTimesVector8(float *V2,const float f,float *V1)
{
	__asm
	{
		movss xmm1,f
		mov eax,V1
		prefetcht1 [eax+32]
		movaps xmm0,[eax]
		shufps xmm1,xmm1,0
		movaps xmm2,[eax+16]
		mov ecx,V2
		mulps xmm0,xmm1
		mulps xmm2,xmm1
		addps xmm0,[ecx]
		addps xmm2,[ecx+16]
		movaps [ecx],xmm0
		movaps [ecx+16],xmm2
	}
}
#else
extern void AddFloatTimesVector8(float *V2,const float f,float *V1);
#endif

inline bool s(float *x,float *NormalX,float *NormalY,float *NormalZ,float *NormalD,unsigned Num,float )
{
	for(unsigned i=0;i<Num;i+=4)
	{        
		ALIGN32 float d1[4];
		ALIGN32 float d2[4];
		ALIGN32 float d3[4];
		for(unsigned j=0;j<4;j++)
		{
			d1[j]=NormalX[i+j]*x[0];
			d2[j]=NormalY[i+j]*x[1];
			d3[j]=NormalZ[i+j]*x[2];
		}
		for(unsigned j=0;j<4;j++)
			d1[j]+=d2[j]+d3[j];
		for(unsigned j=0;j<4;j++)
			if(d1[j]>NormalD[i+j])
				return false;
	}
	return true;
}
extern void SIMUL_MATH_EXPORT_FN YNMultiply(float *y,Matrix &BetaY,unsigned &yn,std::vector<unsigned> *YN1,std::vector<unsigned> *YN2,float *ntemp,Vector &nr);
extern void SIMUL_MATH_EXPORT_FN YNMultiply6(float *y,Matrix &BetaY,unsigned &yn,std::vector<unsigned> *YN1,std::vector<unsigned> *YN2,float *ntemp,Vector &nr);

extern void SIMUL_MATH_EXPORT_FN YNMultiply6a(float *y1,float *y2,Matrix &BetaY,unsigned &yn,unsigned Num,Vector &n_result,unsigned &N);
extern void SIMUL_MATH_EXPORT_FN YNMultiply6b(float *y1,float *y2,Matrix &BetaY,unsigned &yn,Vector &n_result,unsigned &N);


extern void SIMUL_MATH_EXPORT_FN NYMultiply6a(float *y1,float *y2,float *&YVec,unsigned VCN,Vector &n,unsigned &N);
extern void SIMUL_MATH_EXPORT_FN NYMultiply6b(float *y1,float *y2,float *&YVec,Vector &n,unsigned &N);
extern void SIMUL_MATH_EXPORT_FN NYMultiply6(float *y,float *YVec,std::vector<unsigned> *YN,Vector &n);
extern void SIMUL_MATH_EXPORT_FN ReplaceColumns3To5WithCrossProduct(Matrix &Beta);
extern void SIMUL_MATH_EXPORT_FN ApplyForceLimits(Vector &NOutput,Vector &NNormals,
	Vector &ForceMin,Vector &ForceMax,unsigned StartNormalN,unsigned StartFrictionN,
	std::vector<unsigned>&TransverseNSize,std::vector<unsigned>&VCN);

//#undef SIMD
extern void SIMUL_MATH_EXPORT_FN CrossProductRow3To5(float *&MatrixPtr,unsigned rowsize,unsigned num);
extern void SIMUL_MATH_EXPORT_FN ResponseAndB(float *B,float *M1,float *r,float *by,unsigned b_width,unsigned num);
#undef SIMD
}
}
#endif
