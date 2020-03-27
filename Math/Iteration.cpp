#include "Platform/Math/Iteration.h"

#include "VirtualVector.h"
#include "Vector3.h"
#include <math.h>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4740)
#endif
namespace simul
{
	namespace math
	{

void SSORP(Matrix &A,Vector &X,Vector &B,
	int Num,int Lim,Vector &InverseDiagonals)
{
#ifndef SIMD
	int i,j,k;
	for(i=0;i<Num;i++)
	{
		for(j=0;j<(int)X.size;j++)
		{
			float sum=B(j);
			for(k=0;k<(int)X.size;k++)
				//if(N[j]&(1<<k))
					sum+=X(k)*A(j,k);
			X(j)+=sum*InverseDiagonals(j);
		}                                                 
		for(j=0;j<Lim;j++)
			if(X(j)<0)
				X(j)=0;
		for(j=(int)X.size-1;j>=0;j--)
		{
			float sum=B(j);
			for(k=0;k<(int)X.size;k++)
				//if(N[j]&(1<<k))
					sum+=X(k)*A(j,k);
			X(j)+=sum*InverseDiagonals(j);
		}
		for(j=0;j<Lim;j++)
			if(X(j)<0)
				X(j)=0;
	}
#else
	if(!Num)
		return;
	float *A0=A.Values;
	float *X0=X.Values;
	float *B0=B.Values;
	unsigned AStride=4*A.W16;
	float *D0=InverseDiagonals.Values;
	unsigned NSize=X.size;     
	//float *NN=N.Values;
	_asm
	{
		mov eax,Num
		IterationLoop:
			push eax
			mov esi,A0
			mov ebx,B0
			mov edx,D0      
			mov ecx,X0
			mov eax,NSize
			RelaxationLoop1:
				push eax
				push edx
				push ecx
				push esi
				movss xmm2,[ebx]
				mov eax,NSize
				mov edi,X0
				kLoop1:
					movss xmm3,[edi]    // X(k)
					mulss xmm3,[esi]    // A(j,k);
					addss xmm2,xmm3		// sum+=X(k)*A(j,k);
					add edi,4			// X(k++)
					add esi,4			// A(j,k++)
					add eax,-1
					cmp eax,0
				jne kLoop1
				pop esi     
				pop ecx
				pop edx
				pop eax
				movss xmm3,[edx]        // InverseDiagonals(j);
				mulss xmm2,xmm3			// sum*=InverseDiagonals(j);
				addss xmm2,[ecx]        //
				movss [ecx],xmm2        // X(j)+=sum*InverseDiagonals(j);
				add edx,4               // InverseDiagonals(j++)
				add ebx,4               // B(j++)
				add ecx,4               // X(j++)
				add esi,AStride			// A(j++,0)
				add eax,-1
				cmp eax,0
			jne RelaxationLoop1

			// Retain ecx for X(N), ebx for B(N) and esi for A(N,0)
			mov eax,Lim
			cmp eax,0
			je SkipClamp1
			push ecx
			mov ecx,X0
			xorps xmm0,xmm0
			ClampLoop1:
				movss xmm1,[ecx]
				maxss xmm1,xmm0
				movss [ecx],xmm1
				add ecx,4
				add eax,-1
				cmp eax,0
			jne ClampLoop1
			pop ecx
			SkipClamp1:
			mov eax,NSize
			RelaxationLoop2:
				add edx,-4              // D(j--)
				add ebx,-4				// B(j--)
				add ecx,-4				// X(j++)
				sub esi,AStride			// A(j--,0)
				push eax
				push edx
				push ecx
				push esi
				movss xmm2,[ebx]
				mov eax,NSize
				mov edi,X0
				kLoop2:
					movss xmm3,[edi]    // X(k)
					mulss xmm3,[esi]    // A(j,k);
					addss xmm2,xmm3		// sum+=X(k)*A(j,k);
					add edi,4			// X(k++)
					add esi,4			// A(j,k++)
					add eax,-1
					cmp eax,0
				jne kLoop2
				pop esi     
				pop ecx
				pop edx
				pop eax
				movss xmm3,[edx]        // InverseDiagonals(j);
				mulss xmm2,xmm3			// sum*=InverseDiagonals(j);
				addss xmm2,[ecx]        //
				movss [ecx],xmm2        // X(j)+=sum*InverseDiagonals(j);
				add eax,-1
				cmp eax,0
			jne RelaxationLoop2
			    
			mov eax,Lim  
			cmp eax,0
			je SkipClamp2
			mov ecx,X0
			xorps xmm0,xmm0
			ClampLoop2:
				movss xmm1,[ecx]
				maxss xmm1,xmm0  
				movss [ecx],xmm1
				add ecx,4
				add eax,-1
				cmp eax,0
			jne ClampLoop2
        SkipClamp2:
			pop eax
			add eax,-1
			cmp eax,0
		jne IterationLoop
	};
#endif
}

#include <vector>
void SSORP2(Matrix &A,Vector &X,Vector &B,/*std::vector<unsigned __int64> &N,*/
	int Num,int Lim,Vector &InverseDiagonals,void *Nv)
{
#ifndef SIMD
	std::vector<std::vector<unsigned> > &NCorrespondence=*((std::vector<std::vector<unsigned> >*)Nv);
	int i,j,k;
	for(i=0;i<Num;i++)
	{
		for(j=0;j<(int)X.size;j++)
		{
			float sum=B(j);
			unsigned idx=0;
			for(k=0;k<NCorrespondence[j].size();k++)
			{
				idx+=NCorrespondence[j][k]/4;
				sum+=X(idx)*A(j,idx);
			}
			X(j)+=sum*InverseDiagonals(j);
		}                                                 
		for(j=0;j<Lim;j++)
			if(X(j)<0)
				X(j)=0;
		for(j=(int)X.size-1;j>=0;j--)
		{
			float sum=B(j);            
			unsigned idx=0;
			for(k=0;k<NCorrespondence[j].size();k++)
			{
				idx+=NCorrespondence[j][k]/4;
				sum+=X(idx)*A(j,idx);
			}
			X(j)+=sum*InverseDiagonals(j);
		}
		for(j=0;j<Lim;j++)
			if(X(j)<0)
				X(j)=0;
	}
	return;
#else
   //	std::vector<std::vector<unsigned unsigned> > &NCorrespondence=*((std::vector<std::vector<unsigned unsigned> >*)Nv);
	if(!Num)
		return;
	float *A0=A.Values;
	float *X0=X.Values;
	float *B0=B.Values;
	//float *N0=N.Values;
	unsigned AStride=4*A.W16;
	float *D0=InverseDiagonals.Values;
	unsigned NSize=X.size;     
	void *NN;
	_asm
	{
		mov eax,Num
		IterationLoop:
			push eax
			mov esi,A0
			mov ebx,B0
			mov edx,D0      
			mov ecx,X0

			mov eax,Nv                          
			mov eax,[eax]         		// NCorrespondence.Values[0]
			mov NN,eax	         		// NCorrespondence.Values[0]

			mov eax,NSize
			RelaxationLoop1:
				push eax
				push edx
				push ecx
				push esi    
				push ebx
				movss xmm2,[ebx]
				mov edi,X0
				mov ecx,NN      
				mov eax,[ecx+4]
				mov ecx,[ecx]         		// NCorrespondence.Values[j][0]
				kLoop1:
					mov ebx,[ecx]
					add edi,ebx		// X(idx)
					add esi,ebx		// A(j,idx)
					add ecx,4
					movss xmm3,[edi]    // X(k)
					mulss xmm3,[esi]    // A(j,k);
					addss xmm2,xmm3		// sum+=X(k)*A(j,k);
					add eax,-1
					cmp eax,0
				jne kLoop1    
				pop ebx
				pop esi
				pop ecx
				pop edx

				mov eax,NN
				add eax,12
				mov NN,eax              // NCorrespondence.Values[j++]

				pop eax
				movss xmm3,[edx]        // InverseDiagonals(j);
				mulss xmm2,xmm3			// sum*=InverseDiagonals(j);
				addss xmm2,[ecx]        //
				movss [ecx],xmm2        // X(j)+=sum*InverseDiagonals(j);
				add edx,4               // InverseDiagonals(j++)
				add ebx,4               // B(j++)
				add ecx,4               // X(j++)
				add esi,AStride			// A(j++,0)
				add eax,-1
				cmp eax,0
			jne RelaxationLoop1

			// Retain ecx for X(N), ebx for B(N) and esi for A(N,0)
			mov eax,Lim
			cmp eax,0
			je SkipClamp1
			push ecx
			mov ecx,X0
			xorps xmm0,xmm0
			ClampLoop1:
				movss xmm1,[ecx]
				maxss xmm1,xmm0
				movss [ecx],xmm1
				add ecx,4
				add eax,-1
				cmp eax,0
			jne ClampLoop1
			pop ecx
			SkipClamp1:

			mov eax,NSize
			RelaxationLoop2:
				add edx,-4              // D(j--)
				add ebx,-4				// B(j--)
				add ecx,-4				// X(j++)
				sub esi,AStride			// A(j--,0)
				push eax                    

				mov eax,NN
				add eax,-12
				mov NN,eax              // NCorrespondence.Values[j++]

				push edx
				push ecx
				push esi          
				push ebx
				movss xmm2,[ebx]
				mov eax,NSize
				mov edi,X0          
				mov ecx,NN      
				mov eax,[ecx+4]
				mov ecx,[ecx]         		// NCorrespondence.Values[j][0]
				kLoop2:                 
					mov ebx,[ecx]
					add edi,ebx		// X(idx)
					add esi,ebx		// A(j,idx)
					add ecx,4
					movss xmm3,[edi]    // X(k)
					mulss xmm3,[esi]    // A(j,k);
					addss xmm2,xmm3		// sum+=X(k)*A(j,k);
					//add edi,4			// X(k++)
					//add esi,4			// A(j,k++)
					add eax,-1
					cmp eax,0
				jne kLoop2    
				pop ebx
				pop esi
				pop ecx
				pop edx
				pop eax
				movss xmm3,[edx]        // InverseDiagonals(j);
				mulss xmm2,xmm3			// sum*=InverseDiagonals(j);
				addss xmm2,[ecx]        //
				movss [ecx],xmm2        // X(j)+=sum*InverseDiagonals(j);
				add eax,-1
				cmp eax,0
			jne RelaxationLoop2
			    
			mov eax,Lim  
			cmp eax,0
			je SkipClamp2
			mov ecx,X0
			xorps xmm0,xmm0
			ClampLoop2:
				movss xmm1,[ecx]
				maxss xmm1,xmm0  
				movss [ecx],xmm1
				add ecx,4
				add eax,-1
				cmp eax,0
			jne ClampLoop2
        SkipClamp2:
			pop eax
			add eax,-1
			cmp eax,0
		jne IterationLoop
	};
#endif
}
void NYMultiply6a(float *y1,float *y2,float *&YVec,unsigned VCN,Vector &n,unsigned &N)
{
#ifndef SIMD
	for(unsigned i=0;i<VCN;i++)
	{
		float ni=n(N);
		if(y1)
			AddFloatTimesVector8(y1,ni,YVec);
		YVec+=8;
		if(y2)
			AddFloatTimesVector8(y2,ni,YVec);
		YVec+=8;
		N++;
	}
	N+=3;
	N&=~3;
#else
	__asm
	{
		mov edx,VCN
		mov ecx,n
		mov esi,[ecx+4]
		mov eax,N
		mov eax,[eax]		// eax=N
		shl eax,2			// N*4
		add esi,eax			// address of n(N)

		mov eax,YVec
		mov eax,[eax]

		mov ebx,y1
		mov ecx,y2

		cmp ebx,0
		je skip1

		cmp ecx,0
		je skip2

		movaps xmm3,[ebx]
		movaps xmm4,[ebx+16]
		movaps xmm5,[ecx]
		movaps xmm6,[ecx+16]
		
NYLoop:
		prefetcht1 [eax+64]
			movss xmm0,[esi]	// n(N)
			shufps xmm0,xmm0,0

			movaps xmm1,[eax]		// YVec 0,1,2,3
			add esi,4				// Do this early to give time for latency
			movaps xmm2,[eax+16]	// YVec 4,5

			add edx,-1

			mulps xmm1,xmm0
			movaps xmm7,[eax+48]	// YVec 4,5

			mulps xmm2,xmm0
			addps xmm3,xmm1			// +y1 0,1,2,3

			movaps xmm1,[eax+32]	// YVec 0,1,2,3 Do early for latency

			mulps xmm7,xmm0
			addps xmm4,xmm2			// +y1 4,5
			mulps xmm1,xmm0

			add eax,64

			addps xmm6,xmm7			// +y2 4,5
			cmp edx,0
			addps xmm5,xmm1			// +y2 0,1,2,3

		jne NYLoop
		movaps [ebx],xmm3
		movaps [ebx+16],xmm4
		movaps [ecx],xmm5
		movaps [ecx+16],xmm6
							
		jmp finish

skip1:
		movaps xmm5,[ecx]
		movaps xmm6,[ecx+16]
NYLoop1:
			movss xmm0,[esi]		// n(N)
			shufps xmm0,xmm0,0

			movaps xmm1,[eax+32]	// YVec 0,1,2,3
			movaps xmm2,[eax+48]	// YVec 4,5
			mulps xmm1,xmm0
			mulps xmm2,xmm0

			addps xmm5,xmm1			// +y2 0,1,2,3
			addps xmm6,xmm2			// +y2 4,5

			add eax,64
			add esi,4

			add edx,-1
			cmp edx,0
		jne NYLoop1
		movaps [ecx],xmm5
		movaps [ecx+16],xmm6
		jmp finish
skip2:
		movaps xmm3,[ebx]
		movaps xmm4,[ebx+16]
NYLoop2:
			movss xmm0,[esi]		// n(N)
			shufps xmm0,xmm0,0

			movaps xmm1,[eax]		// YVec 0,1,2,3
			movaps xmm2,[eax+16]	// YVec 4,5
			mulps xmm1,xmm0
			mulps xmm2,xmm0

			addps xmm3,xmm1			// +y1 0,1,2,3
			addps xmm4,xmm2			// +y1 4,5

			add eax,64
			add esi,4

			add edx,-1
			cmp edx,0
		jne NYLoop2
		movaps [ebx],xmm3
		movaps [ebx+16],xmm4
		jmp finish
finish:
		mov ebx,YVec
		mov [ebx],eax
	}
	N+=VCN+3;
	N&=~3;
#endif
}
void NYMultiply6b(float *y1,float *y2,float *&YVec,Vector &n,unsigned &N)
{
	float ni=n(N);
	if(y1)
		AddFloatTimesVector8(y1,ni,YVec);
	YVec+=8;
	if(y2)
		AddFloatTimesVector8(y2,ni,YVec);
	YVec+=8;
	N++;
}
void NYMultiply6(float *y,float *YVec,std::vector<unsigned> *YN,Vector &n)
{
#if 1//ndef SIMD
	for(unsigned i=0;i<YN->size();i++)
	{
		unsigned n_index=(*YN)[i];
		float ni=n(n_index);
		AddFloatTimesVector8(y,ni,&(YVec[n_index*16]));
	}
#else
	unsigned YN_NumValues=YN->NumValues;
	unsigned *ynv=YN->Values;
	_asm
	{
		mov eax,y
		mov edx,YN_NumValues		// counter
		movaps xmm0,[eax]
		movaps xmm1,[eax+16]
		mov ecx,ynv
		mov esi,n
		mov esi,[esi+4]	// n.Values
		cmp edx,0
		je escp
			mov ebx,[ecx]	// YN[i]
			mov eax,ebx
			shl ebx,6		// *64, or 16 floats
			add ebx,YVec
			prefetcht2 [ebx]
			shl eax,2
			add eax,esi
			movss xmm4,[eax]
			shufps xmm4,xmm4,0
		floop:


			//shl ebx,6		// *64, or 16 floats
			//add ebx,YVec
			movaps xmm2,[ebx]	//YVec row n_index*2
			add ebx,16
			movaps xmm3,[ebx]	//YVec row n_index*2
			add ecx,4
			dec edx
			mulps xmm2,xmm4
			mov ebx,[ecx]	// YN[i]
			mov eax,ebx
			shl eax,2
			mulps xmm3,xmm4
			shl ebx,6		// *64, or 16 floats
			add ebx,YVec
			prefetcht2 [ebx]
			add eax,esi
			movss xmm4,[eax]
			shufps xmm4,xmm4,0
			cmp edx,0
			addps xmm0,xmm2
			addps xmm1,xmm3
		jg floop
		mov eax,y
		movaps [eax],xmm0
		movaps [eax+16],xmm1
escp:
	}
#endif
}
void YNMultiply(float *y,Matrix &BetaY,unsigned &yn,std::vector<unsigned> *YN1,std::vector<unsigned> *YN2,float *,Vector &nr)
{
	for(unsigned i=0;i<YN1->size();i++)
	{
		float tot=0;
		for(unsigned j=0;j<BetaY.Height;j++)
			tot+=BetaY(j,yn)*y[j];
		yn++;
		//ntemp[i]=tot;
		nr((*YN1)[i])+=tot;
	}
	yn+=3;
	yn&=~3;
	for(unsigned i=0;i<YN2->size();i++)
	{
		float tot=0;
		for(unsigned j=0;j<BetaY.Height;j++)
			tot+=BetaY(j,yn)*y[j];
		yn++;
		//ntemp[i]=tot;
		nr((*YN2)[i])+=tot;
	}
	yn+=3;
	yn&=~3;
}
void YNMultiply6a(float *y1,float *y2,Matrix &BetaY,unsigned &yn,unsigned Num,Vector &n_result,unsigned &N)
{
#ifndef SIMD
	for(unsigned i=0;i<Num;i++)
	{
		float tot=0;
		if(y1)
			for(unsigned j=0;j<6;j++)
				tot+=BetaY(j,yn)*y1[j];
		if(y2)
			for(unsigned j=0;j<6;j++)
				tot+=BetaY(j+6,yn)*y2[j];
		yn++;
		n_result(N)+=tot;
		N++;
	}
	yn+=3;
	yn&=~3;
	N+=3;
	N&=~3;
#else
	unsigned BetaYW16=BetaY.GetAlignedWidth();
	float *Bptr;
	float *nrValues;
	ALIGN16 static float ntemp_[1024];
	float *ytemp=ntemp_;
	static unsigned row_step_3;
	_asm
	{
		mov eax,BetaY
		mov eax,[eax]			// BetaY Values[0]
		mov ecx,yn
		mov ecx,[ecx]
		lea eax,[eax+4*ecx]
		mov Bptr,eax

		mov eax,n_result
		mov eax,[eax+4]
		mov nrValues,eax

		mov ecx,y1				// Y1 loop
		cmp ecx,0
		je skip1
		movss xmm0,[ecx]
		movss xmm1,[ecx+4]
		movss xmm2,[ecx+8]
		movss xmm3,[ecx+12]
		movss xmm4,[ecx+16]
		movss xmm5,[ecx+20]
		shufps xmm0,xmm0,0
		shufps xmm1,xmm1,0
		shufps xmm2,xmm2,0
		shufps xmm3,xmm3,0
		shufps xmm4,xmm4,0
		shufps xmm5,xmm5,0

		mov ebx,ytemp
	/*	movaps [ebx],xmm0
		movaps [ebx+16],xmm1
		movaps [ebx+32],xmm2
		movaps [ebx+48],xmm3
		movaps [ebx+64],xmm4
		movaps [ebx+80],xmm5*/

		mov edx,BetaYW16
		shl edx,2				// Step to get to a new line in BetaY
		mov eax,edx
		imul eax,3
		mov row_step_3,eax		// Step to go 3 lines down in BetaY
		mov esi,Num
		cmp esi,0
		je skip2
		mov ecx,Bptr
		mov eax,N
		mov eax,[eax]
		shl eax,2
		add eax,nrValues
		jLoop1:
			push ecx

			movaps xmm6,[ecx]				// BetaY row
			mulps xmm6,xmm0
			movaps xmm7,[ecx+edx]
			mulps xmm7,xmm1
			addps xmm6,xmm7
			movaps xmm7,[ecx+2*edx]
			mulps xmm7,xmm2
			addps xmm6,xmm7

			add ecx,row_step_3

			movaps xmm7,[ecx]				// BetaY row 3
			mulps xmm7,xmm3
			addps xmm6,xmm7
			movaps xmm7,[ecx+edx]
			mulps xmm7,xmm4
			addps xmm6,xmm7
			movaps xmm7,[ecx+2*edx]
			mulps xmm7,xmm5
			addps xmm6,xmm7

		
			pop ecx
			add ecx,16
			addps xmm6,[eax]				// add to nrValues
			add esi,-4
			add eax,16
			movaps [eax-16],xmm6				// store total to nrValues
			cmp esi,0
		jg jLoop1
	/*	push ecx
		movaps xmm3,[ecx]				// BetaY row
		movaps xmm4,[ecx+edx]
		movaps xmm5,[ecx+2*edx]
		jLoop_high:
			movaps xmm6,[eax]				// nrValues
			add ecx,16

			mulps xmm5,xmm2
			mulps xmm4,xmm1
			mulps xmm3,xmm0

			addps xmm5,xmm4
			addps xmm5,xmm3

		movaps xmm3,[ecx]				// BetaY row
		movaps xmm4,[ecx+edx]
			addps xmm6,xmm5				// add to nrValues
			add eax,16
			add esi,-4
		movaps xmm5,[ecx+2*edx]
		movaps [eax-16],xmm6				// store total to nrValues
			cmp esi,0
		jg jLoop_high
		pop ecx
		add ecx,row_step_3
		movaps xmm0,[ebx+48]
		movaps xmm1,[ebx+64]
		movaps xmm2,[ebx+80]
		mov esi,Num
		jLoop_low:
			push ecx

			movaps xmm3,[ecx]				// BetaY row
			movaps xmm4,[ecx+edx]
			movaps xmm5,[ecx+2*edx]
			pop ecx
			add ecx,16

			mulps xmm5,xmm2
			mulps xmm4,xmm1
			mulps xmm3,xmm0

			addps xmm5,xmm4
			addps xmm5,xmm3

			addps xmm5,[eax]				// add to nrValues
			movaps [eax],xmm5				// store total to nrValues
			add eax,16

			add esi,-4
			cmp esi,0
		jg jLoop_low*/

skip1:
		mov eax,BetaY
		mov eax,[eax]			// BetaY Values[0]
		mov ecx,yn
		mov ecx,[ecx]
		lea eax,[eax+4*ecx]
		mov Bptr,eax

		mov eax,n_result
		mov eax,[eax+4]
		mov nrValues,eax

		mov ecx,y2				// Y1 loop
		cmp ecx,0
		je skip2
		movss xmm0,[ecx]
		movss xmm1,[ecx+4]
		movss xmm2,[ecx+8]
		movss xmm3,[ecx+12]
		movss xmm4,[ecx+16]
		movss xmm5,[ecx+20]
		shufps xmm0,xmm0,0
		shufps xmm1,xmm1,0
		shufps xmm2,xmm2,0
		shufps xmm3,xmm3,0
		shufps xmm4,xmm4,0
		shufps xmm5,xmm5,0

		mov edx,BetaYW16
		shl edx,2				// Step to get to a new line in BetaY
		mov eax,edx
		imul eax,3
		mov row_step_3,eax		// Step to go 3 lines down in BetaY
		mov esi,Num
		mov ecx,Bptr

		shl eax,1				// Step to get to a new line in BetaY
		add ecx,eax				// Row 6 of BetaY
		mov eax,N
		mov eax,[eax]
		shl eax,2
		add eax,nrValues

	/*	mov ebx,ytemp
		movaps [ebx+48],xmm3
		movaps [ebx+64],xmm4
		movaps [ebx+80],xmm5

		push ecx
		movaps xmm3,[ecx]				// BetaY row
		movaps xmm4,[ecx+edx]
		jLoop_high2:
			movaps xmm5,[ecx+2*edx]
			add ecx,16

			mulps xmm5,xmm2
			mulps xmm4,xmm1
			mulps xmm3,xmm0

			addps xmm5,xmm4
			addps xmm5,xmm3

		movaps xmm3,[ecx]				// BetaY row
			addps xmm5,[eax]			// add to nrValues
		movaps xmm4,[ecx+edx]
			add eax,16
		
			add esi,-4
		movaps [eax-16],xmm5			// store total to nrValues
			cmp esi,0
		jg jLoop_high2
		pop ecx
		add ecx,row_step_3
		movaps xmm0,[ebx+48]
		movaps xmm1,[ebx+64]
		movaps xmm2,[ebx+80]
		mov esi,Num
		jLoop_low2:
			push ecx

			movaps xmm3,[ecx]			// BetaY row
			movaps xmm4,[ecx+edx]
			movaps xmm5,[ecx+2*edx]
			pop ecx
			add ecx,16

			mulps xmm5,xmm2
			mulps xmm4,xmm1
			mulps xmm3,xmm0

			addps xmm5,xmm4
			addps xmm5,xmm3

			addps xmm5,[eax]			// add to nrValues
			movaps [eax],xmm5			// store total to nrValues
			add eax,16

			add esi,-4
			cmp esi,0
		jg jLoop_low2*/
		jLoop2:
			push ecx

			movaps xmm6,[ecx]				// BetaY row
			mulps xmm6,xmm0
			movaps xmm7,[ecx+edx]
			mulps xmm7,xmm1
			addps xmm6,xmm7
			movaps xmm7,[ecx+2*edx]
			mulps xmm7,xmm2
			addps xmm6,xmm7

			add ecx,row_step_3

			movaps xmm7,[ecx]				// BetaY row 3
			mulps xmm7,xmm3
			addps xmm6,xmm7
			movaps xmm7,[ecx+edx]
			mulps xmm7,xmm4
			addps xmm6,xmm7
			movaps xmm7,[ecx+2*edx]
			mulps xmm7,xmm5
			addps xmm6,xmm7

			addps xmm6,[eax]				// add to nrValues
			movaps [eax],xmm6				// store total to nrValues
			add eax,16
		
			pop ecx
			add ecx,16
			add esi,-4
			cmp esi,0
		jg jLoop2
skip2:
	}
	yn+=Num;
	yn+=3;
	yn&=~3;
	N+=Num;
	N+=3;
	N&=~3;
#endif
}
void YNMultiply6b(float *y1,float *y2,Matrix &BetaY,unsigned &yn,Vector &n_result,unsigned &N)
{
	float tot=0;
	if(y1)
		for(unsigned j=0;j<6;j++)
			tot+=BetaY(j,yn)*y1[j];
	if(y2)
		for(unsigned j=0;j<6;j++)
			tot+=BetaY(6+j,yn)*y2[j];
	yn++;
	n_result(N)+=tot;
	N++;
}
// y is a pointer to the 6 degrees of freedom.
void YNMultiply6(float *y,Matrix &BetaY,unsigned &yn,
				std::vector<unsigned> *YN1,std::vector<unsigned> *YN2,
				float *ntemp,Vector &nr)
{
	ntemp;
	ALIGN16 float y0[4];
	ALIGN16 float y1[4];
	ALIGN16 float y2[4];
	ALIGN16 float y3[4];
	ALIGN16 float y4[4];
	ALIGN16 float y5[4];
	for(unsigned j=0;j<4;j++)
	{
		y0[j]=y[0];
		y1[j]=y[1];
		y2[j]=y[2];
		y3[j]=y[3];
		y4[j]=y[4];
		y5[j]=y[5];
	}
	for(unsigned j=0;j<YN1->size();j++)
	{
		float tot=0;
		tot+=BetaY(0,yn)*y0[0];
		tot+=BetaY(1,yn)*y1[0];
		tot+=BetaY(2,yn)*y2[0];
		tot+=BetaY(3,yn)*y3[0];
		tot+=BetaY(4,yn)*y4[0];
		tot+=BetaY(5,yn)*y5[0];
		yn++;
		nr((*YN1)[j])+=tot;
	}
	yn+=3;
	yn&=~3;
	for(unsigned j=0;j<YN2->size();j++)
	{
		float tot=0;
		tot+=BetaY(0,yn)*y0[0];
		tot+=BetaY(1,yn)*y1[0];
		tot+=BetaY(2,yn)*y2[0];
		tot+=BetaY(3,yn)*y3[0];
		tot+=BetaY(4,yn)*y4[0];
		tot+=BetaY(5,yn)*y5[0];
		yn++;
		nr((*YN2)[j])+=tot;
	}
	yn+=3;
	yn&=~3;
}


#ifndef SIMD
void AddDotProduct8(float &f,float *V1,float *V2)
{
	for(unsigned i=0;i<8;i++)
		f+=V1[i]*V2[i];
}
#endif


#ifndef SIMD
void AddFloatTimesVector8(float *V2,const float f,float *V1)
{
	for(unsigned i=0;i<8;i++)
		V2[i]+=f*V1[i];
}
#endif
//------------------------------------------------------------------------------
void ReplaceColumns3To5WithCrossProduct(Matrix &Beta)
{
#ifndef SIMD
	VirtualVector cn(4);
	VirtualVector dxn(4);
	VirtualVector bn(8);
	Vector3 TempVector3;
	for(unsigned i=0;i<Beta.Height;i++)
	{
		cn.PointTo(Beta.RowPointer(i));
		dxn.PointTo(Beta.FloatPointer(i,4));
		CrossProduct(TempVector3,dxn,cn);
		bn.PointTo(Beta.RowPointer(i));
		bn(3)=TempVector3(0);
		bn(4)=TempVector3(1);
		bn(5)=TempVector3(2);
		bn(6)=0;
	}
#else
/*
	ONE LINE AT A TIME:
	_asm
	{
		mov eax,Beta
		mov ebx,[eax+12]	// Beta height
		mov eax,[eax]		// Beta Rowpointer(0)
replace_loop:
		movaps xmm1,[eax]	// Cn
		movaps xmm0,[eax+16]	// DXn

		pshufd xmm2,xmm0,0C9h
		mulps xmm2,xmm1
		pshufd xmm3,xmm1,0C9h
		mulps xmm0,xmm3
		subps xmm0,xmm2
		pshufd xmm0,xmm0,0c9h 
		movups [eax+12],xmm0
		add eax,32

		add ebx,-1
		cmp ebx,0
jne replace_loop
	}*/

// IMPROVED? Two lines at once:
	_asm
	{
		mov eax,Beta
		mov ebx,[eax+12]	// Beta height
		mov eax,[eax]		// Beta Rowpointer(0)
replace_loop:
		movaps xmm0,[eax+16]	// DXn
		movaps xmm4,[eax+48]	// DXn
		movaps xmm1,[eax]		// Cn
		movaps xmm5,[eax+32]	// Cn
		prefetcht2 [eax+64]
		pshufd xmm2,xmm0,0C9h
		pshufd xmm6,xmm4,0C9h
		mulps xmm2,xmm1
		mulps xmm6,xmm5
		pshufd xmm3,xmm1,0C9h
		pshufd xmm7,xmm5,0C9h
		mulps xmm0,xmm3
		mulps xmm4,xmm7
		subps xmm0,xmm2
		subps xmm4,xmm6
		pshufd xmm0,xmm0,0c9h 
		pshufd xmm4,xmm4,0c9h 
		add eax,64
		add ebx,-2
		movups [eax-52],xmm0
		movups [eax-20],xmm4
		cmp ebx,0
jne replace_loop
	}
#endif
}

#if 0
void ApplyForceLimits(Vector &NOutput,Vector &NNormals,Vector &ForceMin,
	Vector &ForceMax,unsigned StartNormalN,unsigned StartFrictionN,
	std::vector<unsigned>&TransverseNSize)
{
#ifndef SIMD
	unsigned i;
	for(i=0;i<StartFrictionN;i++)
	{
		if(NOutput(i)<ForceMin(i))
			NOutput(i)=0;
		if(NOutput(i)>ForceMax(i))
			NOutput(i)=ForceMax(i);
	}
	StartNormalN;
	unsigned N=StartFrictionN;
	for(i=0;i<StartFrictionN;i++)
	{
		NNormals[i]=1.f;
		if(!TransverseNSize[i])
			continue;
		float n=NOutput[i];
		if(n>=0)
		{
			NNormals[N]=n;
			N++;
			NNormals[N]=n;
			N++;
			continue;
		}
		NNormals[N]=0;
		N++;
		NNormals[N]=0;
		N++;
	}
	unsigned j=0;
	for(i;i<NOutput.size;i++,j++)
	{                   
		float mn=ForceMin(i)*fabsf(NNormals(i));
		float mx=ForceMax(i)*fabsf(NNormals(i));
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
		i++;
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
	}
#else
	__declspec(align(16)) static unsigned UnsignedMask[]={0x7FFFFFFF,0x7FFFFFFF,0x7FFFFFFF,0x7FFFFFFF};
/*	unsigned N=StartFrictionN;
	for(unsigned i=0;i<StartFrictionN;i++)
	{
		NNormals[i]=1e15f;
		if(!TransverseNSize[i])
			continue;
		float n=NOutput[i];
		if(n>=0)
		{
			NNormals[N]=NOutput[i];
			N++;
			NNormals[N]=NOutput[i];
			N++;
			continue;
		}
		NNormals[N]=0;
		N++;
		NNormals[N]=0;
		N++;
	}*/
	unsigned NSize=NOutput.size;
	float one=1.f;
	_asm
	{
		movss xmm3,one			// xmm3(0)=1.f
		mov eax,NOutput
		mov eax,[eax+4]
		mov edx,StartNormalN
		lea eax,[eax+4*edx]		// Address of normals

		mov ebx,TransverseNSize
		mov ebx,[ebx]

		mov ecx,NNormals
		mov ecx,[ecx+4]
		mov edx,StartFrictionN
		lea ecx,[ecx+4*edx]		// Address of normals

		mov edx,StartFrictionN
		and ecx,~15				//	Normals
push ecx

make_normals_loop:
			movss xmm1,[eax]		// NOutput[i]
			add eax,4				// Next NOutput
			mov esi,[ebx]			// NSize-1
			add ebx,4				// Next NSize
			cmp esi,0				// No transverse components of force
je continue_loop
			movss [ecx],xmm1		// NNormals[N1]=NOutput[i]
			movss [ecx+4],xmm1		// NNormals[N2]=NOutput[i]
			add ecx,8				// Next N1,N2
continue_loop:
			add edx,-1				// Loop Counter
jne make_normals_loop

pop ecx

		mov eax,NOutput
		mov eax,[eax+4]
		mov ebx,ForceMin
		mov ebx,[ebx+4]
		mov esi,ForceMax
		mov esi,[esi+4]
		mov edx,StartFrictionN
		cmp edx,4
		jl normal_tail
normal_limits_loop:
			movaps xmm1,[ebx]		// ForceMin
			movaps xmm2,[esi]		// ForceMax
			add ebx,16
			add esi,16
			movaps xmm0,[eax]		// NOutput
			add edx,-4
			maxps xmm0,xmm1			// zero or greater
			minps xmm0,xmm2			// zero or greater
			add eax,16
			movaps [eax-16],xmm0	// NOutput
			cmp edx,4				// larger than or equal to 4
jge normal_limits_loop
normal_tail:
		cmp edx,0					// aligned
		je friction_aligned

		movaps xmm1,[ebx]			// ForceLimits
		movaps xmm0,[eax]			// NOutput
		maxps xmm0,xmm1				// zero or greater

		movss [eax],xmm0			// first number
		movss [ecx],xmm3			// NNormals[]=1.f
		cmp edx,1					// that's enough
		je friction_unaligned

		shufps xmm0,xmm0,9			// shift right 32 bits
		movss [eax+4],xmm0			// first number
		movss [ecx+4],xmm3			// NNormals[]=1.f

		cmp edx,2					// two is enough
		je friction_unaligned

		shufps xmm0,xmm0,9			// shift right 32 bits
		movss [eax+8],xmm0			// first number
		movss [ecx+8],xmm3			// NNormals[]=1.f

friction_unaligned:
		//and eax,~15				//	NOutput
		//and ebx,~15				//	ForceMin
friction_aligned:

	/*	mov esi,ForceMax
		mov esi,[esi+4]
		mov edx,StartFrictionN
		lea esi,[esi+4*edx]
		and esi,~15				//	ForceMax*/

		mov edx,NSize
		sub edx,StartFrictionN
		cmp edx,0
		jbe skip_all

		movaps xmm7,UnsignedMask
friction_loop:
			movaps xmm0,[ecx]	// Normals
			andps xmm0,xmm7		// fabs(Normals)
			movaps xmm1,[ebx]	// ForceMin
			movaps xmm2,[esi]	// ForceMax
			mulps xmm1,xmm0		// min = Normal * ForceMin
			movaps xmm3,[eax]	// NOutput
			mulps xmm2,xmm0		// max = Normal * ForceMax
			maxps xmm3,xmm1		// NOutput=Max(NOutput,min)
			add ebx,16			// Next 4 ForceMin
			add eax,16			// Next 4 NOutput
			minps xmm3,xmm2		// NOutput=Min(NOutput,max)
			add ecx,16			// Next 4 Normals
			add esi,16			// Next 4 ForceMax
			movaps [eax-16],xmm3// NOutput
			add edx,-4
jg friction_loop
skip_all:
	}
/*	unsigned j=0;
	for(unsigned i=StartFrictionN;i<NOutput.size;i++,j++)
	{                   
		float mn=ForceMin(i)*fabs(NNormals(i));
		float mx=ForceMax(i)*fabs(NNormals(i));
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
		i++;
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
	}*/
#endif
}
#else
void ApplyForceLimits(Vector &NOutput,Vector &NNormals,Vector &ForceMin,
	Vector &ForceMax,unsigned StartNormalN,unsigned ,
	std::vector<unsigned>&TransverseNSize,std::vector<unsigned>&VCN)
{
#ifndef SIMD
	StartNormalN;
	unsigned N=0;
	unsigned i=0;
	for(i=0;i<VCN.size();i++)
	{
		for(unsigned j=0;j<VCN[i];j++,N++)
		{
			NNormals[N]=1.f;
			if(!TransverseNSize[N])
				continue;
			j+=2;
			float n=NOutput[N];
			if(n>=0)
			{
				NNormals[++N]=n;
				NNormals[++N]=n;
				continue;
			}
			NNormals[++N]=0;
			NNormals[++N]=0;
		}
		N+=3;
		N&=~3;
	}
	for(;N<NOutput.size;N++)
	{
		NNormals[N]=1.f;
	}
	//for(i;i<NOutput.size;i++)
	//	NNormals[i]=1.f;
	for(i=0;i<NOutput.size;i++)
	{                   
		float mn=ForceMin(i)*fabsf(NNormals(i));
		float mx=ForceMax(i)*fabsf(NNormals(i));
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
	}
#else
	StartNormalN;
	__declspec(align(16)) static unsigned UnsignedMask[]={0x7FFFFFFF,0x7FFFFFFF,0x7FFFFFFF,0x7FFFFFFF};
/*	unsigned N=0;
	StartNormalN;
	unsigned i=0;
	for(i=0;i<VCN.NumValues;i++)
	{
		for(unsigned j=0;j<VCN[i];j++,N++)
		{
			NNormals[N]=1.f;
			if(!TransverseNSize[N])
				continue;
			j+=2;
			float n=NOutput[N];
			if(n>=0)
			{
				NNormals[++N]=n;
				NNormals[++N]=n;
				continue;
			}
			NNormals[++N]=0;
			NNormals[++N]=0;
		}
		N+=3;
		N&=~3;
	}
	for(N;N<NOutput.size;N++)
	{
		NNormals[N]=1.f;
	}
	*/
	unsigned NSize=NOutput.size;
	unsigned NumVC=VCN.size();
	unsigned *VCNValues=(unsigned *)(&VCN);
	float one=1.f;
	_asm
	{
		movss xmm3,one			// xmm3(0)=1.f
		mov eax,NOutput
		mov eax,[eax+4]
		mov edx,StartNormalN
		lea eax,[eax+4*edx]		// Address of normals

		mov ebx,TransverseNSize
		mov ebx,[ebx]

		mov ecx,NNormals
		mov ecx,[ecx+4]			// Address of normals

		mov edx,NumVC
push ecx
		mov edi,VCNValues
		cmp edx,0
		je non_vc
make_normals_loop1:
			push edx
			mov edx,[edi]
			add edi,4
make_normals_loop2:
				movss [ecx],xmm3		// NNormals[N1]=1
				movss xmm1,[eax]		// NOutput[i]
				add eax,4				// Next NOutput
				mov esi,[ebx]			// NSize-1
				add ebx,4				// Next NSize
				add ecx,4
				cmp esi,0				// No transverse components of force
je continue_loop
				movss [ecx],xmm1		// NNormals[N1]=NOutput[i]
				movss [ecx+4],xmm1		// NNormals[N2]=NOutput[i]
				add ecx,8				// Next N1,N2
				add ebx,4				// Next NSize
continue_loop:
				add edx,-1				// Loop Counter
jne make_normals_loop2
			pop edx
			add edx,-1
jne make_normals_loop1

non_vc:
	mov eax,ecx				// address of next one NNormals[i]
	mov edx,NNormals		// Address of vector
	mov edx,[edx+4]			// Address of normals
	sub eax,edx				// how many bytes?
	shr eax,2				// number of floats
	mov edx,NSize			// NSize
	sub edx,eax;			// NSize-i

non_vc_loop:
		movss [ecx],xmm3		// NNormals[N1]=1
		add ecx,4
		add edx,-1
		cmp edx,0				// No transverse components of force
jne non_vc_loop

pop ecx
		mov eax,NOutput
		mov eax,[eax+4]
		mov ebx,ForceMin
		mov ebx,[ebx+4]
		mov esi,ForceMax
		mov esi,[esi+4]
//friction_unaligned:
		//and eax,~15				//	NOutput
		//and ebx,~15				//	ForceMin
//friction_aligned:

		mov edx,NSize
		cmp edx,0
		jbe skip_all

		movaps xmm7,UnsignedMask
friction_loop:
			movaps xmm0,[ecx]	// Normals
			andps xmm0,xmm7		// fabs(Normals)
			movaps xmm1,[ebx]	// ForceMin
			movaps xmm2,[esi]	// ForceMax
			mulps xmm1,xmm0		// min = Normal * ForceMin
			movaps xmm3,[eax]	// NOutput
			mulps xmm2,xmm0		// max = Normal * ForceMax
			maxps xmm3,xmm1		// NOutput=Max(NOutput,min)
			add ebx,16			// Next 4 ForceMin
			add eax,16			// Next 4 NOutput
			minps xmm3,xmm2		// NOutput=Min(NOutput,max)
			add ecx,16			// Next 4 Normals
			add esi,16			// Next 4 ForceMax
			movaps [eax-16],xmm3// NOutput
			add edx,-4
jg friction_loop
skip_all:
	}
/*	unsigned j=0;
	for(unsigned i=StartFrictionN;i<NOutput.size;i++,j++)
	{                   
		float mn=ForceMin(i)*fabs(NNormals(i));
		float mx=ForceMax(i)*fabs(NNormals(i));
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
		i++;
		if(NOutput(i)<mn)
			NOutput(i)=mn;
		else if(NOutput(i)>mx)
			NOutput(i)=mx;
	}*/
#endif
}
#endif

void CrossProductRow3To5(float *&MatrixPtr,unsigned rowsize,unsigned num)
{
#ifndef SIMD
	for(unsigned i=0;i<num;i++)
	{
		float x1=MatrixPtr[rowsize*3];
		float y1=MatrixPtr[rowsize*4];
		float z1=MatrixPtr[rowsize*5];
		float x2=MatrixPtr[rowsize*0];
		float y2=MatrixPtr[rowsize*1];
		float z2=MatrixPtr[rowsize*2];
		MatrixPtr[rowsize*3]=y1*z2-y2*z1;
		MatrixPtr[rowsize*4]=z1*x2-z2*x1;
		MatrixPtr[rowsize*5]=x1*y2-x2*y1;
		MatrixPtr++;
	}
//	MatrixPtr=(float*)(((unsigned char *)MatrixPtr)+15);
//	MatrixPtr=(float*)(((unsigned char *)MatrixPtr)&(~15));
#else
	_asm
	{
		mov eax,rowsize
		shl eax,2
		mov ebx,MatrixPtr
		mov ebx,[ebx]		// ebx=MatrixPtr
		mov edx,num			
	cploop:
		push ebx
		movaps xmm0,[ebx]		// row 0=x2
		add ebx,eax
		movaps xmm1,[ebx]		// row 1=y2
		add ebx,eax
		movaps xmm2,[ebx]		// row 2=z2
		add ebx,eax
		movaps xmm3,[ebx]		// row 3=x1
		add ebx,eax
		movaps xmm4,[ebx]		// row 4=y1
		add ebx,eax
		movaps xmm5,[ebx]		// row 5=z1
	//
		movaps xmm6,xmm4		// y1
		sub ebx,eax
		sub ebx,eax				// back to row 3
		mulps xmm6,xmm2			// y1*z2

		movaps xmm7,xmm1
		mulps xmm7,xmm5			// y2*z1

		subps xmm6,xmm7
		movaps [ebx],xmm6
		add ebx,eax
	//
		mulps xmm5,xmm0			// z1*x2
		mulps xmm2,xmm3			// z2*x1

		subps xmm5,xmm2
		movaps [ebx],xmm5
		add ebx,eax
	//
		movaps xmm6,xmm3		// x1
		mulps xmm6,xmm1			// x1*y2

		movaps xmm7,xmm0		// x2
		mulps xmm7,xmm4			// x2*y1

		subps xmm6,xmm7
		movaps [ebx],xmm6


		pop ebx
		add ebx,16

		add edx,-4
		cmp edx,0
	jg cploop

	mov eax,MatrixPtr
	mov [eax],ebx			// store new ptr
	};
#endif
}

extern void ResponseAndB(float *B,float *M1,float *r,float *by,unsigned b_width,unsigned num)
{
#ifndef SIMD
	for(unsigned j=0;j<6;j++)
	{
		for(unsigned i=0;i<num;i++)
		{
			float B_row;
			B_row=M1[j]*by[j*b_width+i];
			r[i]+=B_row*by[j*b_width+i];
			B[16*i+j]=B_row;
		}
	}
#else
	for(unsigned j=6;j<6;j++)
	{
		for(unsigned i=0;i<num;i++)
		{
			float B_row;
			B_row=M1[j]*by[j*b_width+i];
			r[i]+=B_row*by[j*b_width+i];
			B[16*i+j]=B_row;
		}
	}
	static float B_space_[1024];			// Like B_row only 4-wide
	static float response_space_[1024];	// Temporary
	float *B_space=B_space_;			// Like B_row only 4-wide
	float *response_space=response_space_;	// Temporary
	_asm
	{
		mov eax,M1
		movaps xmm0,[eax]			// value 0
		mov ebx,by
		mov ecx,B_space
		shufps xmm0,xmm0,0
		movaps xmm7,xmm0			// value 1
		shufps xmm7,xmm7,0x55
		mov edi,response_space
		mov edx,num					// loop
bloop0:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ebx,16
		add ecx,16

		add edx,-4
		cmp edx,0
jg bloop0
		mov edx,num					// loop
		mov ecx,B_space

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
Bt_loop:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop

		mov eax,M1
		movups xmm0,xmm7			// value 0
		mov ebx,by
		mov eax,b_width
		shl eax,2
		add ebx,eax					// row 2
		mov ecx,B_space
		mov edx,num					// loop
bloop1:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ebx,16
		add ecx,16

		add edx,-4
		cmp edx,0
jg bloop1
		mov edx,num					// loop
		mov ecx,B_space

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
		add ebx,4
Bt_loop1:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop1


		mov eax,M1
		movups xmm0,[eax+8]			// value 0
		shufps xmm0,xmm0,0
		mov ebx,by
		mov eax,b_width
		shl eax,2
		imul eax,2
		add ebx,eax					// row 3
		mov ecx,B_space
		mov edx,num					// loop
bloop2:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ebx,16
		add ecx,16

		add edx,-4
		cmp edx,0
jg bloop2
		mov edx,num					// loop
		mov ecx,B_space

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
		add ebx,8					// next column of B
Bt_loop2:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop2


		mov eax,M1
		movups xmm0,[eax+12]			// value 0
		shufps xmm0,xmm0,0
		mov ebx,by
		mov eax,b_width
		shl eax,2
		imul eax,3
		add ebx,eax					// row 4
		mov ecx,B_space
		mov edx,num					// loop
bloop3:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		add ebx,16
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		add edx,-4
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ecx,16

		cmp edx,0
jg bloop3
		mov edx,num					// loop
		mov ecx,B_space

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
		add ebx,12
Bt_loop3:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop3


		mov eax,M1
		movups xmm0,[eax+16]			// value 0
		shufps xmm0,xmm0,0
		mov ebx,by
		mov eax,b_width
		shl eax,2
		imul eax,4
		add ebx,eax					// row 5
		mov ecx,B_space
		mov edx,num					// loop
bloop4:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ebx,16
		add ecx,16

		add edx,-4
		cmp edx,0
jg bloop4

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
		add ebx,16
Bt_loop4:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop4


		mov eax,M1
		movups xmm0,[eax+20]			// value 0
		shufps xmm0,xmm0,0
		mov ebx,by
		mov eax,b_width
		shl eax,2
		imul eax,5
		add ebx,eax					// row 6
		mov ecx,B_space
		mov edx,num					// loop
bloop5:
		movaps xmm1,[ebx]			// beta_y 4 numbers at this row
		movaps xmm2,xmm1			// take a copy
		mulps xmm1,xmm0				// multiply for B_row
		movaps [ecx],xmm1			// store in temporary space
		mulps xmm2,xmm1				// by*Bt
		movaps [edi],xmm2			// store temporary response mult

		add edi,16
		add ebx,16
		add ecx,16

		add edx,-4
		cmp edx,0
jg bloop5

		// B_space now contains N values to go into the column of Bt:
		mov edx,num
		mov ecx,B_space
		mov ebx,B
		add ebx,20
Bt_loop5:
		mov eax,[ecx]
		mov [ebx],eax
		add ecx,4
		add ebx,64
		add edx,-1
		cmp edx,0
jg Bt_loop5

		mov eax,num
		add eax,3
		and eax,~3				// the step
		shl eax,2
		mov ebx,response_space
		mov edx,num
		mov esi,r
rloop:
		movaps xmm1,[ebx]
		mov ecx,ebx
		add ecx,eax				// look at next line
		addps xmm1,[ecx]
		add ecx,eax				// look at next line
		addps xmm1,[ecx]
		add ecx,eax				// look at next line
		addps xmm1,[ecx]
		add ecx,eax				// look at next line
		addps xmm1,[ecx]
		add ecx,eax				// look at next line
		addps xmm1,[ecx]
		movaps xmm2,[esi]
		addps xmm2,xmm1			// add to total
		movaps [esi],xmm2
		add esi,16
		add ebx,16
		add edx,-4
		cmp edx,0
jg rloop
	};
#endif
}
}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
