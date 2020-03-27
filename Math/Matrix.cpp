#define SIM_MATH
#include "MathFunctions.h"
#include "Matrix.h"    
#include "Matrix4x4.h"
#include "SimVector.h"

namespace simul
{
	namespace math
	{

#ifdef PLAYSTATION_2
	#define PLAYSTATION2
	#pragma align(16)
#endif
#ifdef _MSC_VER
	#pragma pack(16)
#define ALIGN16 _declspec(align(16))
struct ALIGN16 float4
{
	float a;
	float b;
	float c;
	float d;
};
#endif
#if defined(_DEBUG) && defined(_CPPUNWIND)
#define CHECK_MATRIX_BOUNDS
#else
#undef CHECK_MATRIX_BOUNDS
#endif
#undef PLAYSTATION2

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

Matrix::Matrix(void)
{
	Values=NULL;      
	BufferSize=0;
#ifdef CANNOT_ALIGN   
	V=NULL;
#endif
	Height=0;
	Width=0;   
	W16=0;
}

Matrix::Matrix(const Matrix &M)
{
	Values=NULL; 
	BufferSize=0;
#ifdef CANNOT_ALIGN   
	V=NULL;
#endif
	Height=0;
	Width=0;   
	W16=0;
	Resize(M.Height,M.Width);
	unsigned i,j;
	for(i=0;i<Height*W16;i++)
		Values[i]=0;
	for(i=0;i<Height;i++)
	{
		for (j=0;j<Width;j++)
		{
			Values[i*W16+j]=M.Values[i*M.W16+j];
		}
	}
}

Matrix::Matrix(unsigned h,unsigned w)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(h<0||w<0)
		throw Matrix::BadSize();
#endif
	Values=NULL;     
	BufferSize=0;
#ifdef CANNOT_ALIGN   
	V=NULL;
#endif
	Height=0;
	Width=0;    
	Resize(h,w);
}

Matrix::Matrix(unsigned h,unsigned w,float val)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(h==0||w==0)
		throw Matrix::BadSize();
#endif
	Values=NULL;
#ifdef CANNOT_ALIGN   
	V=NULL;
#endif
	Height=0;
	Width=0;    
	BufferSize=0;
	Resize(h,w);
	unsigned i,j;
	for(i=0;i<3;i++)
    {
		for(j=0;j<3;j++)
			Values[i*W16+j]=val;
	}
}

Matrix::~Matrix(void)
{
#ifdef CANNOT_ALIGN
	delete[] V;
	V = 0;
#else
	delete[] Values;
	Values = 0;
#endif
}

float * Matrix::Position(const unsigned row,const unsigned col) const
{
	return &(Values[row*W16+col]);
}

void Matrix::ChangeSize(unsigned h,unsigned w)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(h<0||w<0)
		throw Matrix::BadSize();
#endif
	W16=(unsigned)(((unsigned)w+3)&~0x3);
	if(h>Height||w>Width)
	if((unsigned)(((unsigned)h+3)&~0x3)*W16>BufferSize)
	{
#ifdef CANNOT_ALIGN
		delete[] V;
#else
		delete[] Values;
#endif
		unsigned H=(unsigned)(((unsigned)h+3)&~0x3);
		BufferSize=H*W16+8;
#ifdef CANNOT_ALIGN
		V=new float[BufferSize];
		Values=(float*)(((ptr)V+15)&~0xF);
#else
		Values=new float[BufferSize];
#endif
	}
	Height=h;
	Width=w;
	ZeroEdges();
}

void Matrix::ResizeRetainingStep(unsigned h,unsigned w)
{         
#ifdef CHECK_MATRIX_BOUNDS
	if(h<0||w<0)
		throw Matrix::BadSize();
#endif
	if(h==Height&&w==Width)
		return;
	if(h>Height||w>Width)
	{
		if((unsigned)(((unsigned)h+3)&~0x3)*W16>BufferSize)
		{
			unsigned H=(unsigned)(((unsigned)h+3)&~0x3);
			W16=(unsigned)(((unsigned)w+3)&~0x3);
			BufferSize=H*W16+8;
#ifdef CANNOT_ALIGN
			delete[] V;
			V=new float[BufferSize];
			Values=(float*)(((ptr)V+15)&~0xF);
#else
			delete[] Values,*CurrentMemoryPool,16);
			Values=new(*CurrentMemoryPool,16) float[BufferSize];
#endif
		}
		else
		{
			if(w>W16)   
				W16=(unsigned)(((unsigned)w+3)&~0x3);
		}
	}
#ifndef PLAYSTATION2
	for(unsigned i=0;i<(unsigned)(((unsigned)h+3)&~0x3);i++)
	{
		for(unsigned j=w;j<(unsigned)(((unsigned)w+3)&~0x3);j++)
		{
			Values[i*W16+j]=0;
		}
	}
#else
	float *Row=&(Values[w]);
	unsigned Stride=W16*4;
	asm __volatile__
	(
	"
		.set noreorder
		add t0,zero,%0
		xor t1,t1				// t1=0
		xor t7,t7				// t1=0
		add t2,zero,%2			//t2=w
		and t3,t2,3
		beq t3,0,iEscape
		nop
		add t4,%1,3
		and t4,t4,0xFFFFFFFC
		add t5,zero,%4			// t5=Row
		iLoop:	
			add t6,zero,t5
			jLoop:
				sw t7,0(t6)
				addi t6,4
				addi t3,1
			blt t3,4,jLoop
			nop
			add t5,%3
			addi t1,1
		blt t1,t4,iLoop
		nop
		iEscape:
	"	:
		: "g"(this),"g"(h),"g"(w),"g"(Stride),"g"(Row)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1","$f1","$f2"
	);
#endif
	for(unsigned i=h;i<(unsigned)(((unsigned)h+3)&~0x3);i++)
	{         
		Width=w;
		ZeroRow(*this,i);
	}
	Height=h;
	Width=w;   
}

void Matrix::ChangeSizeOnly(unsigned h,unsigned w)
{         
#ifdef CHECK_MATRIX_BOUNDS
	if(h<0||w<0)
		throw Matrix::BadSize();
#endif
	if(h>Height||w>Width)
	{
		if((unsigned)(((unsigned)h+3)&~0x3)*(unsigned)(((unsigned)w+3)&~0x3)>BufferSize)
		{
			unsigned H=(unsigned)(((unsigned)h+3)&~0x3);
			W16=(unsigned)(((unsigned)w+3)&~0x3);
			BufferSize=H*W16+8;
#ifdef CANNOT_ALIGN
			delete[] V;
			V=new float[BufferSize];
			Values=(float*)(((ptr)V+15)&~0xF);
#else
			delete[] Values,*CurrentMemoryPool,16);
			Values=new(*CurrentMemoryPool,16) float[BufferSize];
#endif
		}
		else
		{
			if(w>W16)
				W16=(unsigned)(((unsigned)w+3)&~0x3);    
			else if((unsigned)(((unsigned)h+3)&~0x3)*W16>BufferSize)
				W16=(unsigned)(((unsigned)w+3)&~0x3);
		}
	}     
/*    if(h*W16>BufferSize)
    {
    	unsigned errorhere=1;
        errorhere++;
        errorhere;
    }  */
	Height=h;
	Width=w;
}                  

void Matrix::ChangeSizeRetainingValues(unsigned h,unsigned w)
{         
#ifdef CHECK_MATRIX_BOUNDS
	if(h<0||w<0)
		throw Matrix::BadSize();
#endif
	if(h>Height||w>Width)
	{
		static Matrix Temp;
		if(((unsigned)(((unsigned)h+3)&~0x3)*(unsigned)(((unsigned)w+3)&~0x3)>BufferSize))
		{
			Temp=*this;
			unsigned H=(unsigned)(((unsigned)h+3)&~0x3);
			W16=(unsigned)(((unsigned)w+3)&~0x3);
			BufferSize=H*W16+8;
#ifdef CANNOT_ALIGN
			delete[] V;
			V=new float[BufferSize];
			Values=(float*)(((ptr)V+15)&~0xF);
#else
			delete[] Values,*CurrentMemoryPool,16);
			Values=new(*CurrentMemoryPool,16) float[BufferSize];
#endif
			*this|=Temp;
		}
		else
		{
			if(w>W16||(unsigned)(((unsigned)h+3)&~0x3)*W16>BufferSize)
			{           
				Temp=*this;
				W16=(unsigned)(((unsigned)w+3)&~0x3);     
				*this|=Temp;
			}
		}
	}
	Height=h;
	Width=w;
}

void Matrix::Unit(unsigned row)
{
	if(row)
		Resize(row,row);
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			Values[i*W16+j]=1.0f*(i==j);
		}
	}
#else
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		addi t1,zero,1
		mtc1 t1,$f2
		cvt.s.w $f1,$f2
		lw t0,0(%1)
		lw t4,12(%1)
		lw t5,16(%1)
		xor t3,t3
		blez t4,iEscape
		blez t5,iEscape
		xor t6,t6		// i=0
		vsub vf1,vf0,vf0
		iLoop:
			add t2,zero,t0
			xor t7,t7	// j=0
			jLoop:
				sqc2 vf1,0(t2)
				addi t2,16
				addi t7,4
				sub t8,t5,t7
				bgtz t8,jLoop
			nop
			jEscape:
			add t8,t0,t3
			swc1 $f1,0(t8)
			add t0,t0,%0
			addi t6,1
			addi t3,4
			sub t2,t4,t6
			bgtz t2,iLoop
		nop
		iEscape:
	"	:
		: "g"(Stride),"g"(this)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1","$f1","$f2"
	);
#endif
}
                      
void Matrix::MakeSymmetricFromUpper(void)
{
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=i+1;j<Height;j++)
		{
			Values[j*W16+i]=Values[i*W16+j];
		}
	}
#else	
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		lw t9,12(%0)
		blez t9,Escape
		xor s0,s0
		lw t0,0(%0)					// Result(i,i)
		iLoopSymmetry:
			add s1,zero,s0
			addi s1,1
			add t1,zero,t0			// Result(i,j)
			addi t1,4				// Result(i,j)
			add t2,%1,t0			// Result(j,i)
			jLoopSymmetry:
				lwc1 f1,0(t1)
				swc1 f1,0(t2)
				add t2,%1			// Result(j,i)
				addi s1,1
			blt s1,t9,jLoopSymmetry
				addi t1,4			// Result(i,j)
			add t0,%1
			addi s0,1
		blt s0,t9,iLoopSymmetry
			addi t0,4
		Escape:
	"	:
		: "g"(this),"g"(Stride)
		: "s0","s1","s2","s3","s4","s5","s6","s7"
	);
#endif
}
                      
void Matrix::MakeSymmetricFromLower(void)
{
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=i+1;j<Height;j++)
		{
			Values[i*W16+j]=Values[j*W16+i];
		}
	}
#else	
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		lw t9,12(%0)
		blez t9,Escape
		xor s0,s0
		lw t0,0(%0)					// Result(i,i)
		iLoopSymmetry:
			add s1,zero,s0
			addi s1,1
			add t1,zero,t0			// Result(i,j)
			addi t1,4				// Result(i,j)
			add t2,%1,t0			// Result(j,i)
			jLoopSymmetry:
				lwc1 f1,0(t2)
				swc1 f1,0(t1)
				add t2,%1			// Result(j,i)
				addi s1,1
			blt s1,t9,jLoopSymmetry
				addi t1,4			// Result(i,j)
			add t0,%1
			addi s0,1
		blt s0,t9,iLoopSymmetry
			addi t0,4
		Escape:
	"	:
		: "g"(this),"g"(Stride)
		: "s0","s1","s2","s3","s4","s5","s6","s7"
	);
#endif
}

#ifndef SIMD
void Matrix::ZeroEdges(void)
{
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=Width;j<(unsigned)(((unsigned)Width+3)&~0x3);j++)
        {
			Values[i*W16+j]=0.f;
		}
	}            
	for(i=Height;i<(unsigned)(((unsigned)Height+3)&~0x3);i++)
	{
		for(j=0;j<(unsigned)(((unsigned)Width+3)&~0x3);j++)
        {
			Values[i*W16+j]=0.f;
		}
	}
#else
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		add t2,zero,%0
		lw t4,0(t2)
		lw t0,16(t2)		// Width
		and t0,t0,3         // Width & binary 11
		//cmp t0,0
		beqz t0,SideEscape
		lw t0,16(t2)		// Width
		div t0,t0,4			// Width-1 >> 2
		muli t0,t0,16		// Words
		add t4,t0			// Right-hand edge
		lw t0,12(t2)	 	// t0 = Height
		beqz t0,Escape
		vsub vf1,vf0,vf0
		nop
		Row_Loop1:
			sqc2 vf1,0(t4)
			add t4,Stride
			add t0,t0,-1
		bgtz t0,Row_Loop1
		nop
		SideEscape:
		add t2,zero,%0
		lw t0,12(t2)     	// Height
		and t0,t0,3         // Height & binary 11
		beqz t0,Escape
		lw t0,12(t2)     	// Height
		mul t0,t0,%1                
		lw t3,0(t2)     	// t2=Values
		add t3,t3,t0        // t2=&(Values(Height,0))
		lw t0,12(t2)     	// Height
		and t0,t0,3        	// Height & binary 11
		vsub vf1,vf0,vf0
		Row_Loop:      
			lw t1,16(t2)		// t1=Width
			beqz t1,Escape
			nop
			add t4,zero,t3
			Col_Loop:
				sqc2 vf1,0(t4)
				addi t4,16
				addi t1,-4   
			bgtz t1,Col_Loop
			nop
			add t3,Stride
			addi t0,1
			sub t8,t0,4
		bltz t8,Row_Loop
		nop
		Escape:
		.set reorder
	"	:
		: "g"(this),"g"(Stride)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1","$f1","$f2");        
/*	for(unsigned i=Height;i<(unsigned)(((unsigned)Height+3)&~0x3);i++)
	{
		for(unsigned j=0;j<(unsigned)(((unsigned)Width+3)&~0x3);j++)
        {
			if(V[i*W16+j]!=0)
			{
				unsigned errorhere=1;
				errorhere++;
				errorhere;
	asm __volatile__
	(
	"
		add t2,zero,%0
		lw t0,12(t2)     	// Height
		and t0,t0,3         // Height & binary 11
		beqz t0,Escape2
		lw t0,12(t2)     	// Height
		mul t0,t0,%1                
		lw t3,0(t2)     	// t2=Values
		add t3,t3,t0        // t2=&(Values(Height,0))
		lw t0,12(t2)     	// Height
		and t0,t0,3        	// Height & binary 11
		vsub vf1,vf0,vf0
		Row_Loop2:      
			lw t1,16(t2)		// t1=Width
			beqz t1,Escape
			nop
			add t4,zero,t3
			Col_Loop2:
				sqc2 vf1,0(t4)
				addi t4,16
				addi t1,-4   
			bgtz t1,Col_Loop2
			nop
			add t3,Stride
			addi t0,1
			sub t8,t0,4
		bltz t8,Row_Loop2
		nop
		Escape2:
		.set reorder
	"	:
		: "g"(this),"g"(Stride)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1","$f1","$f2");
			}
		}
	}*/
#endif	
}
#else     
void Matrix::ZeroEdges(void)
{
	unsigned Stride=4*W16;
	_asm
	{
		mov ecx,this
		mov esi,[ecx]
		mov eax,[ecx+16]		// Width
		//dec eax
		and eax,0x3         	// Width & binary 11
		cmp eax,0
		je SideEscape
		mov eax,[ecx+16]		// Width
		shr eax,2				// Width-1 >> 2
		shl eax,4				// Words
		add esi,eax				// Right-hand edge
		mov eax,[ecx+12]        // eax = Height
		cmp eax,0
		je Escape
		xorps xmm0,xmm0
		Row_Loop1:
			movaps [esi],xmm0
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop1
		SideEscape:
		mov ecx,this
		mov eax,[ecx+12]     	// Height
		and eax,0x3         	// Height & binary 11
		cmp eax,0
		je Escape
		mov eax,[ecx+12]     	// Height
		mul Stride              // eax,  
		mov edx,[ecx]     		// ecx=Values
		add edx,eax             // ecx=&(Values(Height,0))
		mov eax,[ecx+12]     	// Height
		and eax,0x3         	// Height & binary 11
		xorps xmm0,xmm0
		Row_Loop:      
			mov ebx,[ecx+16]    // ebx=Width NOT W16!
			cmp ebx,0
			je Escape
			mov esi,edx
			Col_Loop:
				movaps [esi],xmm0
				add esi,16
				sub ebx,4   
				cmp ebx,0
			jg Col_Loop
			add edx,Stride
			inc eax
			cmp eax,4
		jl Row_Loop
		Escape:
	}          
}
#endif          
bool Matrix::CheckEdges(void)
{
	unsigned i,j;
	for(i=0;i<((Height+3)&~3);i++)
	{
		for(j=Width;j<((Width+3)&~3);j++)
		{
			if(Values[i*W16+j]!=0)
			{
				return true;
			}
		}
	}          
	for(i=Height;i<((Height+3)&~3);i++)
	{
		for(j=0;j<((Width+3)&~3);j++)
		{
			if(Values[i*W16+j]!=0)
			{
				return true;
			}
		}
	}
	return false;
}

float Matrix::DiagonalProduct(void)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Height!=Width)
		throw Matrix::BadSize();
#endif
	float result=Values[0];
	for(unsigned i=1;i<Height;i++)
	{
		result*=Values[i*W16+i];
	}
	return result;
}

// Make matrix from vectors
/*void Matrix::RowMatrix(Vector& v)
{
	Resize(1,v.size);
	unsigned i;
	for(i=0;i<Width;i++)
	{
		Values[i]=v[i];
	}
}     */

/*void Matrix::ColumnMatrix(Vector& v)
{
	Resize(v.size,1);
	unsigned i;
	for(i=0;i<Height;i++)
	{
		Values[i*W16]=v[i];
	}
}   */

Matrix Matrix::SubMatrix(unsigned row,const unsigned col,unsigned h,unsigned w)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row+h>Height||col+w>Width||row<0||col<0||h<0||w<0)
		throw Matrix::BadSize();
#endif
	Matrix M(h,w);
	unsigned i,j;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			M.Values[i*M.W16+j]=Values[(row+i)*W16+col+j];
		}
	}
	return M;
}

Matrix operator*(const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
	Matrix ret(M1.Height,M2.Width);
#ifdef CHECK_MATRIX_BOUNDS
	if(M1.Width!=M2.Height)
		throw Matrix::BadSize();
#endif
	for(i=0;i<M1.Height;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			ret(i,j)=0.f;
			for(k=0;k<M1.Width;k++)
			{
				ret(i,j)+=M1(i,k)*M2(k,j);
			}
		}
	}
	return ret;
}      

void Multiply(Matrix &M,const Matrix &A,const Matrix &B)
{          
#ifdef CHECK_MATRIX_BOUNDS
	if(A.Width!=B.Height)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
	if(M.Height!=A.Height)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif
#ifdef SIMD
	unsigned BStride=B.W16*4;
	unsigned MStride=M.W16*4;
	unsigned AStride=A.W16*4;
	unsigned W=A.Width;
	unsigned H=A.Height;
	unsigned Remainder=M.Width;
	float *Result=(float *)(M.Values);
	float *APtr=(float *)(A.Values);
	float *B0=(float *)(B.Values);
	float *BPtr;
//	M.CheckEdges();
	_asm
	{
		mov edx,H
		Row_Loop:
			mov edi,dword ptr[B0]   
			mov dword ptr[BPtr],edi
			mov edi,dword ptr[Result]
			mov esi,Remainder      
			mov ebx,dword ptr[BPtr]
			Column_Loop:
				mov eax,dword ptr[APtr]
				xorps xmm6,xmm6
				mov ecx,W
				Dot_Product_Loop:
				// Load four Values from A into xmm1
					movaps xmm1,[eax]
	//movups [edx],xmm1
				// Load one value from B into xmm2
					movss xmm2,[ebx]  
	//movups [edx],xmm2
				// look down one line
					add ebx,BStride
				// Load the 2nd value from B into xmm3
					movss xmm3,[ebx]  
	//movups [edx],xmm3
				// look down to the 3rd line
					add ebx,BStride
				// Load the 3rd value from B into xmm4
					movss xmm4,[ebx] 
	//movups [edx],xmm4
				// look down to the 4th line
					add ebx,BStride
				// Load the 4th value from B into xmm5
					movss xmm5,[ebx]    
	//movups [edx],xmm5
				// now shuffle these last three Values into xmm2:
					shufps xmm2,xmm3,0x44
					shufps xmm2,xmm2,0x88
					shufps xmm4,xmm5,0x44
					shufps xmm4,xmm4,0x88
					shufps xmm2,xmm4,0x44         
   //	movups [edx],xmm2
			// Now xmm2 contains the four Values from B.
			// Multiply the four Values from A and B:
					mulps xmm1,xmm2
	//movups [edx],xmm1
			// Now add the four multiples to the four totals in xmm6:
					addps xmm6,xmm1
	//movups [edx],xmm6
					add eax,16
					add ebx,BStride
					sub ecx,4
					cmp ecx,3
				jg Dot_Product_Loop
				cmp ecx,0
				jle NoTail

				Tail_Loop:
					movss xmm1,[eax]
					movss xmm2,[ebx]
					mulss xmm1,xmm2   
					addss xmm6,xmm1
					add eax,4
					add ebx,BStride
					sub ecx,1
				jg Tail_Loop
				
				NoTail:
	//movups [edx],xmm1
	//movups [edx],xmm6
			// Now add the four sums together:
				shufps xmm1,xmm6,0xEE
	//movups [edx],xmm1
				shufps xmm1,xmm1,0xEE
	//movups [edx],xmm1
				addps xmm6,xmm1
	//movups [edx],xmm6
				shufps xmm1,xmm6,0x11
	//movups [edx],xmm1
				shufps xmm1,xmm1,0xEE
	//movups [edx],xmm1
				addss xmm6,xmm1
	//movups [edx],xmm6
	//pop edx
				movss [edi],xmm6
			// Add up the next M value
				add edi,4
			// Look at the next column of B:
				mov ebx,dword ptr[BPtr]
				add ebx,4       
				mov dword ptr[BPtr],ebx
				dec esi
				cmp esi,0
			jne Column_Loop
			mov edi,dword ptr[Result]
			mov eax,MStride
			add edi,eax
			mov dword ptr[Result],edi   
			mov edi,dword ptr[APtr]
			mov eax,AStride
			add edi,eax
			mov dword ptr[APtr],edi
			//pop edx
			dec edx
			cmp edx,0
		jne Row_Loop
	}                
//	M.CheckEdges();
//	if(M.CheckNAN())
//		throw BadNumber();
#else
#ifdef PLAYSTATION2
	unsigned BStride=B.W16*4;			
	unsigned MStride=M.W16*4;
	unsigned AStride=A.W16*4;
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
					"g"(BStride),"g"(A.Width),"g"(M.Width),"g"(A.Height)
				:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
					"$vf1", "$vf2", "$vf3", "$vf4", "$vf5","$vf10"
			);
//    M.CheckEdges();
#else
	unsigned i,j,k;
	for(i=0;i<A.Height;i++)
	{
		for(j=0;j<M.Width;j++)
		{
			float *m1row=&A.Values[i*A.W16+0];
			float t=0.f;
			k=0;
			unsigned l=A.Width;
			while(l>4)
			{
				t+=m1row[k+0]*B.Values[(k+0)*B.W16+j];
				t+=m1row[k+1]*B.Values[(k+1)*B.W16+j];
				t+=m1row[k+2]*B.Values[(k+2)*B.W16+j];
				t+=m1row[k+3]*B.Values[(k+3)*B.W16+j];
				k+=4;
				l-=4;
			}
			switch(l)
			{
				case 4:	t+=m1row[k]*B(k,j);	++k;  // fall into 321 case
				case 3:	t+=m1row[k]*B(k,j);	++k;
				case 2:	t+=m1row[k]*B(k,j);	++k;
				case 1:	t+=m1row[k]*B(k,j);	break;
				case 0: break;
			}
			M(i,j)=t;
		}
	}
//    M.CheckEdges();
#endif
#endif     
}

#ifdef SIMD
void MultiplyMatrixByTranspose(Matrix &M,const Matrix &A,const Matrix &B)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(A.Width!=B.Width)
		throw Matrix::BadSize();
	if(M.Height!=A.Height)
		throw Matrix::BadSize();
	if(M.Width!=B.Height)
		throw Matrix::BadSize();
	if(M.Width==0||M.Height==0)
		throw Matrix::BadSize();
#endif
	//for(unsigned i=0;i<M.Height;++i)
	//{
	//	for(unsigned j=0;j<B.Height;++j)
	//	{
	//		M(i,j)=0.f;
	//		for(unsigned k=0;k<A.Width;k++)
	//		{
	//			M(i,j)+=A(i,k)*B.Values[j*B.W16+k];
	//		}
	//	}
	//}
	// The Stride is the number of BYTES between successive rows:
	unsigned BStride=B.W16*4;
	unsigned MStride=M.W16*4;
	unsigned AStride=A.W16*4;
	unsigned W=((A.Width+3)&~0x3)>>2;
	unsigned H=A.Height;
	float *Result=(float *)(M.Values);
	float *APtr=(float *)(A.Values);
   //	float *B0=(float *)(B.Values);
	float *BPtr;
	_asm
	{
		mov edx,H
		cmp edx,0
		je iEscape
		mov ecx,W
		cmp ecx,5
		jne Row_Loop
		
		Row_Loop5:
			mov edi,B     
			mov esi,[edi+12]
			mov edi,[edi]
			mov BPtr,edi
			mov edi,dword ptr[Result]
			mov ebx,dword ptr[BPtr]    
			mov eax,APtr
			movaps xmm0,[eax]
			movaps xmm1,[eax+16]
			movaps xmm2,[eax+32]
			movaps xmm3,[eax+48]
			movaps xmm4,[eax+64]
			Column_Loop5:     // the ROWS of B   
				movaps xmm5,[ebx]
				mulps xmm5,xmm0
				movaps xmm6,[ebx+16]
				mulps xmm6,xmm1
				addps xmm5,xmm6
				movaps xmm6,[ebx+32]
				mulps xmm6,xmm2
				addps xmm5,xmm6
				movaps xmm6,[ebx+48]
				mulps xmm6,xmm3
				addps xmm5,xmm6
				movaps xmm6,[ebx+64]
				mulps xmm6,xmm4
				addps xmm5,xmm6
			// Now add the four sums together:
				movhlps xmm6,xmm5
				addps xmm5,xmm6         // = X+Z,Y+W,--,--
				movaps xmm6,xmm5
				shufps xmm6,xmm6,5      // Y+W,--,--,--
				addss xmm5,xmm6
				movss [edi],xmm5
			// Add up the next M value
				add edi,4
			// Look at the next column of B:
				mov ebx,BPtr
				add ebx,BStride
				mov BPtr,ebx
				dec esi
				cmp esi,0
			jne Column_Loop5
			mov edi,Result
			add edi,MStride
			mov Result,edi
			mov edi,APtr
			add edi,AStride
			mov APtr,edi
			dec edx
			cmp edx,0
		jne Row_Loop5     
		jmp iEscape
/*		mov edx,H

		mov eax,M
		mov eax,[eax]
		mov Result,eax
		mov eax,A   
		mov eax,[eax]
		mov APtr,eax */

		Row_Loop:
			mov edi,B     
			mov esi,[edi+12]
			mov edi,[edi]
			mov BPtr,edi
			mov edi,dword ptr[Result]
			mov ebx,dword ptr[BPtr]
			Column_Loop:     // Or in this case, the ROWS of B
				mov eax,APtr
				xorps xmm2,xmm2
				mov ecx,W
				Dot_Product_Loop:
			// Load four Values from A into xmm1
					movaps xmm1,[eax]
			// Multiply the four Values from A and B:
					mulps xmm1,[ebx]
			// Now add the four multiples to the four totals in xmm2:
					addps xmm2,xmm1
					add eax,16
					add ebx,16
					sub ecx,1
				jnz Dot_Product_Loop
			// Now add the four sums together:
				movhlps xmm1,xmm2
				addps xmm2,xmm1         // = X+Z,Y+W,--,--
				movaps xmm1,xmm2
				shufps xmm1,xmm1,1      // Y+W,--,--,--
				addss xmm2,xmm1
				movss [edi],xmm2
			// Add up the next M value
				add edi,4
			// Look at the next column of B:
				mov ebx,BPtr
				add ebx,BStride
				mov BPtr,ebx
				sub esi,1
			jnz Column_Loop
			mov edi,Result
			add edi,MStride
			mov Result,edi
			mov edi,APtr
			add edi,AStride
			mov APtr,edi
			sub edx,1
		jnz Row_Loop
		iEscape:
	}
}
#else
#ifdef PLAYSTATION2
void MultiplyMatrixByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2)
{
// Get a dot-product of rows R1 and R2 of Matrices M1 and M2.
// On Playstation 2, we will do this by taking two groups of four numbers
// at a time from each row. We multiply them together to get a single group
// of four numbers, and we add each result.
// Finally, we add the four sums together to obtain the single dot-product.
	static float d[4];
	float *dd=d;
	float *Row1=(M1.Values);
	float *Row2=(M2.Values);
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	unsigned Stride3=M.W16*4;
	float *ResultRow=M.Values;
	asm __volatile__("
		.set noreorder  
		add t4,zero,%4
		blez t4,Escape	// for zero height matrix 
		nop     
		add t9,zero,%0				// Start of Row i of M
		iLoop:							// Rows of M1 and M
			add t0,zero,t9				// Start of Row i of M
			add t7,zero,%2 				// Row zero of M2
			xor t5,t5					// Repeat times H for columns
			jLoop:   
				add t1,zero,%1				// Start of Row i of M1
				add t3,zero,%8			// Width of M1 and M2
				add t2,zero,t7 			// Row j of M2
				vsub vf4,vf0,vf0
				kLoop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmul vf3,vf1,vf2
					vadd vf4,vf4,vf3
					addi t1,16			// next four numbers
					addi t3,-4			// counter-=4
				bgtz t3,kLoop
					addi t2,16
// Now vf4 contains 4 sums to be added together.
				vaddx vf5,vf4,vf4x
				vaddy vf5,vf4,vf5y
				vaddz vf5,vf4,vf5z  	// So the sum is in the w.
				vmul vf5,vf5,vf0		// Now it's 3 zeroes and the sum.
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
				addi t0,4				// Next column of M
				add t7,t7,%6			// Next Row of M2
				addi t5,1				// column counter j=j+1
				sub t6,%3,t5			// Width of M - j
			bgtz t6,jLoop
			nop
			add t9,%7				// Next Row of M
			add %1,%1,%5				// Next Row of M1
			addi t4,-1					// row counter
		bgtz t4,iLoop
		nop		
		Escape:
	"					: 
						: "r" (ResultRow),"r" (Row1), "r" (Row2), "r" (M.Width), "r" (M.Height),
							"r" (Stride1), "r" (Stride2), "r" (Stride3), "r" (M1.Width)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
}
#else
void MultiplyMatrixByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
	for(i=0;i<M1.Height;++i)
	{
		for(j=0;j<M2.Height;++j)
		{
			M.Values[i*M.W16+j]=0.f;
			for(k=0;k<M1.Width;k++)
			{
				M.Values[i*M.W16+j]+=M1.Values[i*M1.W16+k]*M2.Values[j*M2.W16+k];
			}
		}
	}
}
#endif                 
#endif

void Multiply(Matrix &M,const Matrix &A,const Matrix &B,unsigned Column)
{
	unsigned i,j,k;
	for(i=0;i<A.Height;i++)
	{
		for(j=0;j<B.Width;j++)
		{
			M(i,Column+j)=0.f;
			for(k=0;k<A.Width;k++)
			{
				M(i,Column+j)+=A(i,k)*B(k,Column+j);
			}
		}
	}
}            

void TransposeMultiplyByTranspose(Matrix &M,const Matrix &A,const Matrix &B,unsigned Column)
{
#ifndef PLAYSTATION2
	unsigned i,j,k;
	for(i=0;i<A.Height;i++)
	{
		for(j=0;j<B.Height;j++)
		{
			M(j,Column+i)=0.f;
			for(k=0;k<A.Width;k++)
			{
				M(j,Column+i)+=A(i,k)*B(j,Column+k);
			}
		}
	}
#else
	unsigned BStride=B.W16*4;
	unsigned RStride=M.W16*4;
	unsigned AStride=A.W16*4;
	float *Result=(float *)&(M.Values[Column]);
	float *APtr=(float *)(A.Values);
	float *BPtr=(float *)&(B.Values[Column]);
	asm __volatile__("
		.set noreorder
		add t3,zero,%6
		blez t3,iEscape
		nop
		Row_Loop:
			add t4,zero,%0			// M(0,Column+i)
			add t5,zero,%8			// B.Height
			add s3,zero,%2			// B(0,Column+0)
			Column_Loop:
				add t0,zero,%1		// ptr to A.Values[row i]
				add t1,zero,s3		// B(j,Column+0)
				sub.s f4,f4
				add t2,zero,%7		// A.Width
				Dot_Product_Loop:
					lwc1 f1,0(t0)
					lwc1 f2,0(t1)
					mul.s f3,f1,f2
					add.s f4,f4,f3
					addi t0,4		// A(i,k)
					addi t1,4		// B
					addi t2,-1
				bgtz t2,Dot_Product_Loop
				nop
				swc1 f4,0(t4)
				add t4,%3			// M(j,Column+i)
				add s3,%5			// B(j,Column+0)
				addi t5,-1
			bnez t5,Column_Loop
			nop
			addi %0,4				// M(0,Column+i)
			add %1,%4				// A(i,0)
			addi t3,-1
		bnez t3,Row_Loop
		nop
		iEscape:
	"					: 
						: "g"(Result),"g"(APtr),"g"(BPtr),"g"(RStride),"g"(AStride),
							"g"(BStride),"g"(A.Height),"g"(A.Width),"g"(B.Height)
						: "s0", "s1", "s2", "s3", "s4", "s5");
#endif
}

void MultiplyByTranspose(Matrix &M,const Matrix &A,const Matrix &B,unsigned Column)
{
#ifndef SIMD
#if 1//ndef PLAYSTATION2
	unsigned i,j,k;
	for(i=0;i<A.Height;i++)
	{
		for(j=0;j<B.Height;j++)
		{
			M(Column+i,j)=0.f;
			for(k=0;k<A.Width;k++)
			{
				M(Column+i,j)+=A(i,k)*B(j,Column+k);
			}
		}
	}
#else
	unsigned BStride=B.W16*4;
	unsigned MStride=M.W16*4;
	unsigned AStride=A.W16*4;
	float *Result=(float *)&(M.Values[Column*M.W16]);
	float *APtr=(float *)(A.Values);
	float *B0=(float *)&(B.Values[Column]);
	float *BPtr=(float *)&(B.Values[Column]);
	asm __volatile__("
		.set noreorder
		add t4,zero,%1
		lw t3,12(t4)
		beq t3,0,iEscape
		nop
		Row_Loop:
			add t4,zero,%2
			add t5,zero,%5
			add %6,zero,t5
			add t5,zero,%10
			add t4,zero,%3
			Column_Loop:     // Or in this case, the ROWS of %2
				add t0,zero,%4    
				add t1,zero,%6
				sub.s $f4,$f4
				lw t2,16(%1)
				Dot_Product_Loop:
			// Load four Values from %1 into xmm1
					lwc1 $f1,0(t0)
					lwc1 $f2,0(t1)
			// Multiply the four Values from A and B:
					mul.s $f3,$f1,$f2
			// Now add the four multiples to the four totals in xmm2:
					add.s $f4,$f4,$f3
			//movss tmp,xmm2
					addi t0,4
					addi t1,4
					addi t2,-1
				bgt t2,0,Dot_Product_Loop
				nop
				swc1 $f4,0(t4)
			// Add up the next M value
				addi t4,4
			// Look at the next column of B:
				add t1,zero,%6
				add t1,%9
				add %6,zero,t1
				addi t5,-1
			bnez t5,Column_Loop
			nop
			add t4,zero,%3
			add t4,%7
			add %3,zero,t4
			add t4,zero,%4
			add t4,%8
			add %4,zero,t4
			addi t3,-1
		bnez t3,Row_Loop
		nop
		iEscape:
	"					: 
						: "g"(&M),"g"(&A),"g"(&B),"g"(Result),"g"(APtr),
							"g"(B0),"g"(BPtr),"g"(MStride),"g"(AStride),"g"(BStride),
							"g"(A.Height)
						: "s0", "s1", "s2", "s3", "s4", "s5");
#endif
#else
	unsigned BStride=B.W16*4;
	unsigned MStride=M.W16*4;
	unsigned AStride=A.W16*4;
	unsigned W=A.Width;
 //	unsigned AH=A.Height;
//float tmp;
//	unsigned BH=B.Height;
	float *Result=(float *)&(M.Values[Column*M.W16]);
	float *APtr=(float *)(A.Values);
	float *B0=(float *)&(B.Values[Column]);
	float *BPtr=(float *)&(B.Values[Column]);
	_asm
	{         
		mov edi,A
		mov edx,[edi+12]
		cmp edx,0
		je iEscape
		Row_Loop:
			mov edi,B
			mov esi,B0
			mov BPtr,esi
			mov esi,[edi+12]
			mov edi,Result
			Column_Loop:     // Or in this case, the ROWS of B
				mov eax,APtr    
				mov ebx,BPtr
				xorps xmm2,xmm2
				mov ecx,W
				Dot_Product_Loop:
			// Load four Values from A into xmm1
					movss xmm1,[eax]
			//movss tmp,xmm1
			// Multiply the four Values from A and B:
					mulss xmm1,[ebx]
			//movss tmp,xmm1
			// Now add the four multiples to the four totals in xmm2:
					addss xmm2,xmm1
			//movss tmp,xmm2
					add eax,4
					add ebx,4
					sub ecx,1
					cmp ecx,0
				jg Dot_Product_Loop
			// Now add the four sums together:
				/*movhlps xmm1,xmm2
				addps xmm2,xmm1         // = X+Z,Y+W,--,--
				movaps xmm1,xmm2
				shufps xmm1,xmm1,1      // Y+W,--,--,--
				addss xmm2,xmm1  */
				movss [edi],xmm2
			// Add up the next M value
				add edi,4
			// Look at the next column of B:
				mov ebx,BPtr
				add ebx,BStride
				mov BPtr,ebx
				sub esi,1
			jnz Column_Loop
			mov edi,Result
			add edi,MStride
			mov Result,edi
			mov edi,APtr
			add edi,AStride
			mov APtr,edi
			sub edx,1
		jnz Row_Loop
		iEscape:
	}
#endif
}

void MultiplyDiagonalByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j;
	for(i=0;i<M1.Height;i++)
	{
		for(j=0;j<M2.Height;j++)
		{
			M(i,j)=M1(i,i)*M2(j,i);
		}
	}
}

void MultiplyDiagonalByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2,unsigned Column)
{
	unsigned i,j;
	for(i=0;i<M1.Height;i++)
	{
		for(j=0;j<M2.Height;j++)
		{
			M(Column+i,j)=M1(i,i)*M2(j,Column+i);
		}
	}
}
   
float MultiplyRows(Matrix &M,unsigned R1,unsigned R2)
{
#ifndef SIMD          
	float Result;
	unsigned i;
	Result=0.f;
	for(i=0;i<M.Width;++i)
		Result+=M(R1,i)*M(R2,i);
	return Result;
#else                   
	float Result;
	float *r=&Result;
	_asm
	{
		mov esi,M
		mov eax,[esi+8]
		mov edi,[esi]

		imul eax,4		//MStride
		mov ecx,eax

		mul R2
		mov ebx,edi
		add ebx,eax

		mov eax,ecx
		mul R1
		add eax,edi

		xorps xmm2,xmm2
		mov ecx,[esi+8]
		cmp ecx,4
		jl Tail
		Dot_Product_Loop:
			// Load four Values from A into xmm1
			movaps xmm1,[eax]
			// Multiply the four Values from A and B:
			mulps xmm1,[ebx]
			// Now add the four multiples to the four totals in xmm2:
			addps xmm2,xmm1
			add eax,16
			add ebx,16
			add ecx,-4
			cmp ecx,3
		jg Dot_Product_Loop
		Tail:
		cmp ecx,0
		je AddUp
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm2,xmm1
		cmp ecx,1
		je AddUp
			add eax,4
			add ebx,4
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm2,xmm1   
		cmp ecx,2
		je AddUp
			add eax,4
			add ebx,4
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm2,xmm1
		AddUp:
		// Now add the four sums together:
		movhlps xmm1,xmm2
		addps xmm2,xmm1         // = X+Z,Y+W,--,--
		movaps xmm1,xmm2
		shufps xmm1,xmm1,1      // Y+W,--,--,--
		addss xmm2,xmm1
		mov esi,r;
		movss [esi],xmm2
	}               
	return Result;
#endif
}
  
int t1[]={-1,0,0,0};
int t2[]={-1,-1,0,0};
int t3[]={-1,-1,-1,0};
int t4[]={-1,-1,-1,-1};
#ifndef SIMD
float MultiplyRows(Matrix &M1,unsigned R1,Matrix &M2,unsigned R2)
{
	unsigned i;
	float Result=0.f;
	for(i=0;i<M1.Width;++i)
		Result+=M1(R1,i)*M2(R2,i);  
	return Result;
}
#else
float MultiplyRows(Matrix &M1,unsigned R1,Matrix &M2,unsigned R2)
{
#if 1
	float Result;
	//float *r=&Result;
	_asm
	{
		mov esi,M2
		mov eax,[esi+8]
		mov edi,[esi]

		imul eax,4		// MStride
		mov ecx,eax

		mul R2
		mov ebx,edi
		add ebx,eax
                    
		mov esi,M1
		mov eax,[esi+8]
		mov edi,[esi]

		mov eax,ecx
		mul R1
		add eax,edi

		xorps xmm0,xmm0
		mov ecx,[esi+16]

		cmp ecx,4
		jl Tail
		jLoop:
			movaps xmm1,[eax]
			mulps xmm1,[ebx]
			addps xmm0,xmm1
			add eax,16
			add ebx,16
			add ecx,-4
			cmp ecx,3
		jg jLoop
		Tail:
		cmp ecx,0
		je AddUp
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm0,xmm1
		cmp ecx,1
		je AddUp
			movss xmm1,[eax+4]
			mulss xmm1,[ebx+4]
			addss xmm0,xmm1
		cmp ecx,2
		je AddUp
			movss xmm1,[eax+8]
			mulss xmm1,[ebx+8]
			addss xmm0,xmm1
		AddUp:
		// Now add the four sums together:
		movhlps xmm1,xmm0
		addps xmm0,xmm1         // = X+Z,Y+W,--,--
		movaps xmm1,xmm0
		shufps xmm1,xmm1,1      // Y+W,--,--,--
		addss xmm0,xmm1

		movss Result,xmm0

		//Escape:
	}
	return Result;
#else
	float Result;
	float *r=&Result;
	unsigned *tt1=t1;
	unsigned *tt2=t2;
	unsigned *tt3=t3;
	unsigned *tt4=t4;   
	unsigned *tt=NULL;
	_asm
	{       
		mov esi,M1
		mov eax,[esi+16]  
		cmp eax,0
		je Escape
		and eax,3
		cmp eax,0
		je Mask4  
		cmp eax,3
		je Mask3
		cmp eax,2
		je Mask2
		Mask1:
			mov ecx,tt1
			jmp EndMask
		Mask2:
			mov ecx,tt2
			jmp EndMask
		Mask3:
			mov ecx,tt3
			jmp EndMask
		Mask4:
			mov ecx,tt4
		EndMask:     
		mov tt,ecx
                      
		mov esi,M2
		mov eax,[esi+8]
		mov edi,[esi]

		imul eax,4		// MStride
		mov ecx,eax

		mul R2
		mov ebx,edi
		add ebx,eax
                    
		mov esi,M1
		mov eax,[esi+8]
		mov edi,[esi]

		mov eax,ecx
		mul R1
		add eax,edi

		xorps xmm0,xmm0
		mov ecx,[esi+16]

		cmp ecx,16
		jl End_Macro_Loop
                   
		Macro_Loop:
			movaps xmm1,[eax]
			movaps xmm2,[eax+16]
			movaps xmm3,[eax+32]
			movaps xmm4,[eax+48]
			mulps xmm1,[ebx]
			mulps xmm2,[ebx+16]
			mulps xmm3,[ebx+32]
			mulps xmm4,[ebx+48]
			addps xmm0,xmm1
			addps xmm0,xmm2
			addps xmm0,xmm3   
			addps xmm0,xmm4
			add eax,64
			add ebx,64
			add ecx,-16   
			cmp ecx,15
		jg Macro_Loop
	   End_Macro_Loop:
		cmp ecx,4
		jle Tail
		Dot_Product_Loop:
			movaps xmm1,[eax]
			mulps xmm1,[ebx]
			addps xmm0,xmm1
			add eax,16
			add ebx,16
			add ecx,-4
			cmp ecx,4
		jg Dot_Product_Loop
		Tail:
		/*cmp ecx,0
		je AddUp
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm0,xmm1
		cmp ecx,1
		je AddUp
			add eax,4
			add ebx,4
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm0,xmm1
		cmp ecx,2
		je AddUp
			add eax,4
			add ebx,4
			movss xmm1,[eax]
			mulss xmm1,[ebx]
			addss xmm0,xmm1*/   
		movaps xmm1,[eax]
		mulps xmm1,[ebx]
		mov ecx,tt
		movups xmm2,[ecx]
		andps xmm1,xmm2
		addps xmm0,xmm1
		AddUp:
		// Now add the four sums together:
		movhlps xmm1,xmm0
		addps xmm0,xmm1         // = X+Z,Y+W,--,--
		movaps xmm1,xmm0
		shufps xmm1,xmm1,1      // Y+W,--,--,--
		addss xmm0,xmm1
		mov esi,r;
		movss [esi],xmm0
		//Escape:
	}
	return Result;
#endif
}
#endif

void Multiply(Matrix &M,const Matrix &M1,const Matrix &M2,unsigned rows,unsigned elements,unsigned columns)
{
	unsigned i,j,k;
	for(i=0;i<rows;i++)
	{
		for(j=0;j<columns;j++)
		{
			float *m1row = &M1.Values[i*M1.W16+0];
			float t=0.f;
			k = 0;
			unsigned l=elements;
			while(l > 4) {
					t+=m1row[k+0]*M2.Values[k+0*M2.W16+j];
					t+=m1row[k+1]*M2.Values[k+1*M2.W16+j];
					t+=m1row[k+2]*M2.Values[k+2*M2.W16+j];
					t+=m1row[k+3]*M2.Values[k+3*M2.W16+j];
					k+=4;
					l-=4;
			}
			switch(l) {
				case 4:	t+=m1row[k]*M2(k,j);	++k;  // fall into 321 case
				case 3:	t+=m1row[k]*M2(k,j);	++k;
				case 2:	t+=m1row[k]*M2(k,j);	++k;
				case 1:	t+=m1row[k]*M2(k,j);	break;
				case 0: break;
			}
			M.Values[i*M.W16+j]=t;
		}
	}
}

void Matrix::InsertMultiply(const Matrix &M1,const Matrix &M2,const unsigned row,const unsigned col)
{      
	unsigned i,j,k;   
#ifdef CHECK_MATRIX_BOUNDS
	if(row+M1.Height>Height||col+M2.Width>Width)
		throw Matrix::BadSize();
#endif
	for(i=0;i<M1.Height;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			Values[(row+i)*W16+col+j]=0.f;
			for(k=0;k<M1.Width;k++)
			{
				Values[(row+i)*W16+col+j]+=M1(i,k)*M2(k,j);
			}
		}
	}
}

/*void Matrix::AddOuter(const Vector &v1,const Vector &v2)
{
	unsigned i,j;
	for(i=0;i<v1.size;i++)
	{
		for(j=0;j<v2.size;j++)
		{
			Values[i*W16+j]+=v1.Values[i]*v2.Values[j];
		}
	}
} */

/*void Matrix::SubtractOuter(const Vector &v1,const Vector &v2)
{
	unsigned i,j;
	for(i=0;i<v1.size;i++)
	{
		for(j=0;j<v2.size;j++)
		{
			Values[i*W16+j]-=v1.Values[i]*v2.Values[j];
		}
	}
}  */

Matrix& Matrix::operator=(const Matrix &M)
{
	Resize(M.Height,M.Width);
	(*this)|=M;
	return *this;
}

Matrix& Matrix::operator=(const float *m)
{
	for(size_t i=0;i<Height*Width;i++)
		Values[i]=m[i];
	return *this;
}
       
void Matrix::operator|=(const Matrix &M)
{         
#ifdef CHECK_MATRIX_BOUNDS
	if(Width<M.Width||Height<M.Height)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}      
	if(Width==0||Height==0)
		throw Matrix::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i,j;
	for(i=0;i<M.Height;i++)
		for(j=0;j<M.Width;j++)
			Values[i*W16+j]=M.Values[i*M.W16+j];
#else
	unsigned Stride=4*W16;
	unsigned MStride=4*M.W16;
	asm __volatile__
	(
	"
		.set noreorder
		lw t0,0(%3)
		lw t1,0(%2)
		lw t4,12(%2)
		lw t5,16(%2)
		blez t4,iEscape
		blez t5,iEscape
		xor t6,t6		// i=0
		iLoop:
			add t2,zero,t0
			add t3,zero,t1
			xor t7,t7	// j=0
			jLoop:
				lqc2 vf1,0(t3)
				sqc2 vf1,0(t2)
				addi t2,16
				addi t7,4
			blt t7,t5,jLoop
				addi t3,16
			jEscape:
			add t0,t0,%0
			addi t6,1
		blt t6,t4,iLoop
			add t1,t1,%1
		iEscape:
		.set reorder
	"	:
		: "g"(Stride),"g"(MStride),"g"(&M),"g"(this)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1"
	);
#endif
#else
	unsigned MStride=4*M.W16;
	unsigned Stride=4*W16;
	//float *val=Values;
	//float *Mval=M.Values;
	if(!M.Values)
		return;
	//unsigned H1=M.Height;
	//unsigned W1=M.Width;
	_asm
	{
		mov ecx,M   
		mov eax,this
		mov esi,[eax]
		mov edi,[ecx]//dword ptr[Mval]
		mov eax,[ecx+12]//H1
		Row_Loop:
			mov ebx,[ecx+16]//W1
			push esi
			push edi
			Col_Loop:
				movaps xmm1,[edi]
				add edi,16
				sub ebx,4
				movaps [esi],xmm1
				add esi,16
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

void Matrix::operator+=(const Matrix &M)
{
#ifndef SIMD
unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(Width!=M.Width||Height!=M.Height)
		throw Matrix::BadSize();
#endif
	for (i=0;i<Height; i++)
	{
		for (j=0;j<Width;j++)
		{
			Values[i*W16+j]+=M.Values[i*M.W16+j];
		}
	}
#else
	unsigned MStride=4*M.W16;
	unsigned Stride=4*W16;
	_asm
	{
		mov ecx,this
		mov ebx,M
		mov esi,[ecx]
		mov edi,[ebx]
		mov eax,[ecx+12]
		Row_Loop:      
			mov ebx,[ecx+16]
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

void Matrix::operator-=(const Matrix &M)
{
#ifndef SIMD
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if (Width!=M.Width||Height!=M.Height)
		throw Matrix::BadSize();
#endif
	for(i=0;i<Height;i++)
	{
		for (j=0;j<Width;j++)
		{
			Values[i*W16+j]=Values[i*W16+j]-M.Values[i*M.W16+j];
		}
	}
#else
	unsigned MStride=4*M.W16;
	unsigned Stride=4*W16;
	float *val=Values;
	float *Mval=M.Values;
	if(!Mval)
		return;
	unsigned H1=M.Height;
	unsigned W1=M.Width;
	_asm
	{                     
		mov esi,val
		mov edi,Mval
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

#ifndef SIMD
void Matrix::Unit(void)
{
	unsigned i,j;
Zero();
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			Values[i*W16+j]=1.f*(i==j);
		}
	}
}
#else
void Matrix::Unit(void)
{
	unsigned Stride=4*W16;
	if(!Values)
		return;
	unsigned H1=Height;
	_asm
	{
		mov ecx,this
		mov esi,[ecx]
		mov edi,esi
		mov eax,H1
		xorps xmm1,xmm1    
		fld1
		Row_Loop:
			mov ebx,[ecx+16]
			push esi
			Col_Loop:
				movaps [esi],xmm1
				add esi,16
				sub ebx,4
				cmp ebx,0
			jg Col_Loop
			fst [edi]
			add edi,Stride
			add edi,4
			pop esi
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop  
		mov edi,[ecx]
		fstp [edi]
	}
}
#endif

void Matrix::Clear(void)
{
	Height=0;
	Width=0;
}                    

void Matrix::ClearMemory(void)
{          
#ifdef CANNOT_ALIGN
	delete[] V;  
	V=NULL;
#else
	delete[] Values,*CurrentMemoryPool,16);
#endif     
	BufferSize=0;
	Values=NULL;
	Height=0;
	Width=0;
	W16=0;
}

void Matrix::Transpose(void) 
{
	Matrix temp(Width,Height);
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			temp.Values[j*temp.W16+i]=Values[i*W16+j];
		}
	}
	Resize(Width,Height);
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			Values[i*W16+j]=temp.Values[i*temp.W16+j];
		}
	}
}                          

void Matrix::Transpose(Matrix &M) const
{
	unsigned i,j;
	for(i=0;i<Height;i++)
		for(j=0;j<Width;j++)
			M.Values[j*M.W16+i]=Values[i*W16+j];
}

#ifndef PLAYSTATION2
#ifndef SIMD
void Matrix::Zero(void)
{
	unsigned i,j;
	for(i=0;i<((Height+3)&~3);i++)
	{
		for(j=0;j<W16;j++)
		{
			Values[i*W16+j]=0.f;
		}
	}
}
#else
//#include <memory.h>
void Matrix::Zero(void)
{
	unsigned Stride=4*W16;
	_asm
	{
		mov ebx,this
		mov ecx,[ebx+8]
		mov eax,[ebx+12]
		mul ecx
		mov ecx,eax
		xor eax,eax
		mov edi,[ebx]
		rep stos eax
		Escape:
	}
}
#endif
#else
void Matrix::Zero(void)
{
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		lw t0,0(a0)
		lw t4,12(a0)
		lw t5,16(a0)
		blez t4,iEscape
		blez t5,iEscape
		xor t6,t6		// i=0
		vsub vf1,vf0,vf0
		iLoop:
			add t2,zero,t0
			xor t7,t7	// j=0
			jLoop:
				sqc2 vf1,0(t2)
				addi t7,4
			blt t7,t5,jLoop
				addi t2,16
			jEscape:
			addi t6,1
		blt t6,t4,iLoop
			add t0,t0,%0
		iEscape:
		.set reorder
	"	:
		: "g"(Stride)//,"g"(this)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1"
	);
}
#endif

void Matrix::Zero(unsigned size)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i,j;
	for(i=0;i<((size+3)&~3);i++)
	{
		for(j=0;j<((size+3)&~3);j++)
		{
			Values[i*W16+j]=0.f;
		}
	}
#else
	unsigned Stride=4*W16;
	unsigned *sz=&size;
	_asm
	{
		mov ecx,this
		mov edx,[ecx]
		mov eax,dword ptr[sz]
		mov eax,[eax]
		cmp eax,0
		je Escape
		xorps xmm0,xmm0
		Row_Loop:      
			mov ebx,dword ptr[sz]
			mov ebx,[ebx]
			mov esi,edx
			Col_Loop:
				movaps [esi],xmm0
				add esi,16
				sub ebx,4   
				cmp ebx,0
			jg Col_Loop
			add edx,Stride
			dec eax
			cmp eax,0
		jg Row_Loop
		Escape:
	}
#endif
#else
	unsigned Stride=4*W16;
	asm __volatile__
	(
	"
		.set noreorder
		lw t0,0(a0)
		add t8,zero,%1
		blez t8,iEscape
		xor t6,t6		// i=0
		vsub vf1,vf0,vf0
		iLoop:
			add t2,zero,t0
			xor t7,t7	// j=0
			jLoop:
				sqc2 vf1,0(t2)
				addi t7,4
			blt t7,t8,jLoop
				addi t2,16
			jEscape:
			addi t6,1
		blt t6,t8,iLoop
			add t0,t0,%0
		iEscape:
		.set reorder
	"	:
		: "g"(Stride),"g"(size)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1"
	);
#endif
}


void Matrix::InsertMatrix(const Matrix &M,const unsigned row,const unsigned col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row+M.Height>Height||col+M.Width>Width||row<0||col<0)
	{
		throw Matrix::BadSize();
	}
#endif
#ifndef SIMD
#ifndef PLAYSTATION2    
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		for(j=0;j<M.Width;j++)
		{
			Values[(row+i)*W16+col+j]=M.Values[i*M.W16+j];
		}
	}
#else
	if((col&3)!=0)
	{
		unsigned i,j;
		for(i=0;i<M.Height;i++)
		{
			for(j=0;j<M.Width;j++)
			{
				Values[(row+i)*W16+col+j]=M.Values[i*M.W16+j];
			}
		}
		return;
	}
	unsigned Stride=4*W16;
	unsigned MStride=4*M.W16;
	//unsigned AWidth=M.Width;
	//unsigned AHeight=M.Height;
	float *ValuesAddr=&(Values[row*W16+col]);
	float *MValuesAddr=M.Values;
	asm __volatile__
	(
	"
		.set noreorder
		lw t0,0(%3)
		add t3,zero,%4
		mul t3,t3,%0
		add t0,t0,t3
		muli t3,%5,16
		add t0,t0,t3
		lw t1,0(%2)
		lw t4,12(%2)
		lw t5,16(%2)
		blez t4,iEscape
		blez t5,iEscape
		xor t6,t6		// i=0
		iLoop:
			add t2,zero,t0
			add t3,zero,t1
			xor t7,t7	// j=0
			jLoop:
				lqc2 vf1,0(t3)
				sqc2 vf1,0(t2)
				addi t2,16
				addi t7,4
			blt t7,t5,jLoop
				addi t3,16
			jEscape:
			add t0,t0,%0
			addi t6,1
		blt t6,t4,iLoop
			add t1,t1,%1
		iEscape:
		.set reorder
	"	:
		: "r"(Stride),"g"(MStride),"g"(&M),"g"(this),"g"(row),"g"(col)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1"
	);
#endif
#else
	//static unsigned mask[8];
	//unsigned *m=(unsigned *)(((unsigned)mask+0xF)&~0xF);
	unsigned MStride=4*M.W16;
	unsigned Stride=4*W16;
	float *val=&(Values[row*W16+col]);
	float *Mval=M.Values;
	if(!Mval)
		return;
	//float temp1[4];
	//float *temp=temp1;
	unsigned H1=M.Height;
	unsigned W1=M.Width;
	_asm
	{                     
		mov esi,val
		mov edi,dword ptr[Mval]   
		mov ecx,W1
		mov edx,H1
		cmp ecx,0
		je Escape
		Row_Loop:
			push esi
			push edi
			mov ebx,W1
			cmp ebx,4
			jl Side
			mov eax,esi
			and eax,15
			cmp eax,0
			jne Side
			Col_Loop:
				movaps xmm1,[edi]
				movaps [esi],xmm1
				add edi,16
				add esi,16
				sub ebx,4
				cmp ebx,3
			jg Col_Loop
			cmp ebx,0
			je Next
			Side:
			Col_Loop_Unaligned:
				movss xmm1,[edi]
				movss [esi],xmm1
				add edi,4
				add esi,4
				sub ebx,1
				cmp ebx,0
			jg Col_Loop_Unaligned
			Next:
			pop edi
			pop esi
			add edi,MStride
			add esi,Stride
			dec edx
			cmp edx,0
		jg Row_Loop
		Escape:
	}            
/*
static unsigned mask[8];
	unsigned *m=(unsigned *)(((unsigned)mask+0xF)&~0xF);
	unsigned MStride=4*M.W16;
	unsigned Stride=4*W16;
	float *val=&(Values[row*W16]);
	float *Mval=M.Values;
	if(!Mval)
		return;
	float temp1[4];
	float *temp=temp1;
	unsigned H1=M.Height;
	unsigned W1=M.Width;
	_asm
	{                     
		mov esi,val
		mov edi,dword ptr[Mval]   
		mov eax,W1
		and eax,3
		mov ecx,eax
		mov eax,H1
		cmp ecx,0
		je Row_Loop
		mov ebx,0xFFFF
		mov edx,m
		xorps xmm1,xmm1
		movaps [edx],xmm1
		mov [edx+12],ebx
		cmp ecx,3
		je Row_Loop  
		mov [edx+8],ebx
		cmp ecx,2
		je Row_Loop    
		mov [edx+4],ebx
		cmp ecx,1
		je Row_Loop
		Row_Loop:
			mov ebx,W1
			push esi
			push edi  
			cmp ebx,4
			jl Side
			Col_Loop:
				movaps xmm1,[edi]
				movaps [esi],xmm1
				add edi,16
				add esi,16
				sub ebx,4
				cmp ebx,3
			jg Col_Loop
			cmp ebx,0
			je Next
			Side:
				mov ecx,temp
			movaps xmm1,[esi]	// All 4
				movups [ecx],xmm1
			andps xmm1,[edx]    // rhs    
				movups [ecx],xmm1
			movaps xmm2,[edx]	// All 4
				movups [ecx],xmm2
			andnps xmm2,[edi]
				movups [ecx],xmm2
			addps xmm1,xmm2        
				movups [ecx],xmm1
			movaps [esi],xmm1
			Next:
			pop edi
			pop esi
			add edi,MStride
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop
	}

*/
#endif
}

void Matrix::InsertMatrix(const Matrix4x4 &M,const unsigned row,const unsigned col)
{
#ifndef SIMD
	unsigned i,j;
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			Values[(row+i)*W16+col+j]=M.Values[i*4+j];
		}
	}
#else
	unsigned MStride=4*4;
	unsigned Stride=4*W16;
	float *val=&(Values[row*W16+col]);
	const float *Mval=M.Values;
	if(!Mval)
		return;
	unsigned H1=4;
	unsigned W1=4;
	_asm
	{                     
		mov esi,val
		mov edi,dword ptr[Mval]   
		mov ecx,W1
		mov edx,H1
		cmp ecx,0
		je Escape
		Row_Loop:
			push esi
			push edi
			mov ebx,W1
			cmp ebx,4
			jl Side
			mov eax,esi
			and eax,15
			cmp eax,0
			jne Side
			Col_Loop:
				movaps xmm1,[edi]
				movaps [esi],xmm1
				add edi,16
				add esi,16
				sub ebx,4
				cmp ebx,3
			jg Col_Loop
			cmp ebx,0
			je Next
			Side:
			Col_Loop_Unaligned:
				movss xmm1,[edi]
				movss [esi],xmm1
				add edi,4
				add esi,4
				sub ebx,1
				cmp ebx,0
			jg Col_Loop_Unaligned
			Next:
			pop edi
			pop esi
			add edi,MStride
			add esi,Stride
			dec edx
			cmp edx,0
		jg Row_Loop
		Escape:
	}
#endif
}

void Matrix::InsertTranspose(const Matrix &M2,const unsigned row,const unsigned col)
{
#ifndef SIMD
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(row+M2.Width>Height||col+M2.Height>Width||row<0||col<0)
	{
		throw Matrix::BadSize();
	}
#endif
	for(i=0;i<M2.Width;i++)
	{
		for(j=0;j<M2.Height;j++)
		{
			Values[(row+i)*W16+col+j]=M2.Values[j*M2.W16+i];
		}
	}     
#else
	unsigned Stride=W16*4;
	unsigned MStride=M2.W16*4;
	_asm
	{
		mov ecx,this
		mov esi,[ecx]
		mov eax,row
		mul Stride
		add esi,eax
		mov eax,col
		imul eax,4
		add esi,eax     
		mov edx,M2
		mov edi,[edx]
		mov ebx,[edx+16]	// W
		iLoop:
			push edi
			push esi    
			mov eax,[edx+12]	// H
			jLoop:
				movss xmm1,[edi]
				movss [esi],xmm1
				add edi,MStride
				add esi,4
				dec eax
				cmp eax,0
			jne jLoop
			pop esi
			add esi,Stride
			pop edi
			add edi,4     
			dec ebx
			cmp ebx,0
		jne iLoop
	}
#endif
}

void Matrix::InsertTranspose(const Matrix4x4 &M2,const unsigned row,const unsigned col)
{
#ifndef SIMD
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(row+4>Height||col+4>Width||row<0||col<0)
	{
		throw Matrix::BadSize();
	}
#endif
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			Values[(row+i)*4+col+j]=M2(j,i);
		}
	}     
#else
	unsigned Stride=W16*4;
	unsigned MStride=4*4;
	_asm
	{
		mov ecx,this
		mov esi,[ecx]
		mov eax,row
		mul Stride
		add esi,eax
		mov eax,col
		imul eax,4
		add esi,eax     
		mov edx,M2
		mov edi,[edx]
		mov ebx,[edx+16]	// W
		iLoop:
			push edi
			push esi    
			mov eax,[edx+12]	// H
			jLoop:
				movss xmm1,[edi]
				movss [esi],xmm1
				add edi,MStride
				add esi,4
				dec eax
				cmp eax,0
			jne jLoop
			pop esi
			add esi,Stride
			pop edi
			add edi,4     
			dec ebx
			cmp ebx,0
		jne iLoop
	}
#endif
}

void Matrix::InsertAddMatrix(const Matrix &M2,const unsigned row,const unsigned col)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(row+M2.Height>Height||col+M2.Width>Width||row<0||col<0)
	{
		throw Matrix::BadSize();
	}
#endif
	for(i=row;i<row+M2.Height;i++)
	{
		for(j=col;j<col+M2.Width;j++)
		{
			Values[i*W16+j]+=M2.Values[i-row*M2.W16+j-col];
		}
	}
}

void Matrix::InsertSubMatrix(const Matrix &M,unsigned InsRow,unsigned InsCol,const unsigned row,const unsigned col,const unsigned h,const unsigned w)
{
	unsigned i,j;     
	#ifdef CHECK_MATRIX_BOUNDS
	if(InsRow+h>Height||InsCol+w>Width||InsRow<0||InsCol<0)
		throw Matrix::BadSize();
	if(row+h>M.Height||col+w>M.Width||row<0||col<0||h<0||w<0)
		throw Matrix::BadSize();  
	#endif
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			Values[(InsRow+i)*W16+InsCol+j]=M.Values[(row+i)*M.W16+col+j];
		}
	}
}

// Insert the specified number rows and columns of matrix M from the top left
// into InsRow,InsCol of this matrix

void Matrix::InsertPartialMatrix(const Matrix &M,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w)
{
	unsigned i,j;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			Values[(InsRow+i)*W16+InsCol+j]=M.Values[i*M.W16+j];
		}
	}
}

void Matrix::InsertNegativePartialMatrix(const Matrix &M,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w)
{
	unsigned i,j;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			Values[(InsRow+i)*W16+InsCol+j]=-M.Values[i*M.W16+j];
		}
	}
}                        

void Matrix::InsertSubtractPartialMatrix(const Matrix &M,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w)
{
	unsigned i,j;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			Values[(InsRow+i)*W16+InsCol+j]-=M.Values[i*M.W16+j];
		}
	}
}

void Matrix::InsertAddVectorTimesSubMatrix(const Vector &V,const Matrix &M,const unsigned Row,
	const unsigned Col,const unsigned w)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=Height||Col+w>Width||Row<0||Col<0)
		throw Matrix::BadSize();
	if(V.size!=M.Height)
		throw Matrix::BadSize();
#endif
	for(i=0;i<w;i++)
	{
		float &Sum=Values[Row*W16+Col+i];
		for(j=0;j<V.size;j++)
		{
			Sum+=V.Values[j]*M.Values[j*M.W16+i];
		}
		Values[Row*W16+Col+i]=Sum;
	}
}

void Matrix::InsertAddVectorTimesMatrix(const Vector &V,const Matrix &M,unsigned Row,unsigned Col)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=Height||Col+M.Width>Width||Row<0||Col<0)
		throw Matrix::BadSize();
	if(V.size!=M.Height)
		throw Matrix::BadSize();
#endif
	for(i=0;i<M.Width;i++)
	{
		float &Sum=Values[Row*W16+Col+i];
		for(j=0;j<V.size;j++)
		{
			Sum+=V(j)*M.Values[j*M.W16+i];
		}
	}
}

void Matrix::InsertSubtractVectorTimesMatrix(const Vector &V,const Matrix &M,unsigned Row,unsigned Col)
{
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=Height||Col+M.Width>Width||Row<0||Col<0)
		throw Matrix::BadSize();
	if(V.size!=M.Height)
		throw Matrix::BadSize();
#endif
	for(i=0;i<M.Width;i++)
	{
		float &Sum=Values[Row*W16+Col+i];
		for(j=0;j<V.size;j++)
		{
			Sum-=V(j)*M.Values[j*M.W16+i];
		}
	}
}

void Matrix::InsertVectorAsColumn(const Vector &v,const unsigned col)
{
	for(unsigned i=0;i<Height;i++)
		Values[i*W16+col]=v.Values[i];
}

void Matrix::InsertNegativeVectorAsColumn(const Vector &v,const unsigned col)
{
	for(unsigned i=0;i<Height;i++)
		Values[i*W16+col]=-v.Values[i];
}

void Matrix::InsertCrossProductAsColumn(const Vector &v1,const Vector &v2,const unsigned col)
{                        
#ifdef CHECK_MATRIX_BOUNDS
	if(col>=Width||col<0)
		throw Matrix::BadSize();   
#endif
	Values[0*W16+col]=v1.Values[1]*v2.Values[2]-v2.Values[1]*v1.Values[2];
	Values[1*W16+col]=v1.Values[2]*v2.Values[0]-v2.Values[2]*v1.Values[0];
	Values[2*W16+col]=v1.Values[0]*v2.Values[1]-v2.Values[0]*v1.Values[1];
}
  
#ifdef SIMD
void Matrix::AddScalarTimesMatrix(float val,const Matrix &M2)
{
	unsigned MStride=4*M2.W16;
	unsigned Stride=4*W16;
	float *v=&val;
	//float t[4];
	//float *tt=t;
	_asm
	{
		mov ecx,this
		mov ebx,M2
		mov esi,[ecx]
		mov edi,[ebx]
		mov eax,[ecx+12]
		mov edx,v
		movss xmm0,[edx]
		shufps xmm0,xmm0,0
		Row_Loop:
			mov ebx,[ecx+16]
			push esi
			push edi
			Col_Loop:
				movaps xmm2,[edi]
				//movaps xmm1,[esi]
				mulps xmm2,xmm0
				//addps xmm1,xmm2
                	addps xmm2,[esi]
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
}
#else
#ifdef PLAYSTATION2
void Matrix::AddScalarTimesMatrix(float val,const Matrix &M2)
{
	unsigned i,j;
	for(i=0;i<M2.Height;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			Values[i*W16+j]+=val*M2.Values[i*M2.W16+j];
		}
	}
}
#else
void Matrix::AddScalarTimesMatrix(float val,const Matrix &M2)
{
	unsigned i,j;
	for(i=0;i<M2.Height;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			Values[i*W16+j]+=val*M2.Values[i*M2.W16+j];
		}
	}
}
#endif
#endif

void Matrix::SubtractScalarTimesMatrix(float val,const Matrix &M2)
{
	unsigned i,j;
	for(i=0;i<M2.Height;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			Values[i*W16+j]-=val*M2.Values[i*M2.W16+j];
		}
	}
}

Matrix Matrix::operator/(const float d)
{
	unsigned i,j;
	Matrix temp(Height,Width);
	for (i=0;i<Height; i++)
	{
		for (j=0;j<Width;j++)
		{
			temp.Values[i*temp.W16+j]=Values[i*W16+j]/d;
		}
	}
	return temp;
}

Vector Matrix::operator*(const Vector &v)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Width!=v.size)
		throw Matrix::BadSize();
#endif
	Vector v2(Height);
	if(Width==v.size)
	{
		unsigned i,j;
		for(i=0;i<Height;i++)
		{
			float val=0.0;
			for(j=0;j<v.size;j++)
			{
				val+=Values[i*W16+j]*v(j);
			}
			v2(i)=val;
		}
	}
	return v2;
}

// Multiply by scalar:

Matrix operator*(float d,const Matrix &M)
{
   	unsigned i,j;
	Matrix temp(M.Height,M.Width);

	for (i=0;i<M.Height; i++)
	{
		for (j=0;j<M.Width;j++)
		{
			temp.Values[i*temp.W16+j]=M.Values[i*M.W16+j]*d;
		}
	}
	return temp;
}                     

Matrix &Matrix::operator*=(float d)
{
	unsigned i,j;
	for (i=0;i<Height;i++)
	{
		for (j=0;j<Width;j++)
		{
			Values[i*W16+j]*=d;
		}
	}
	return *this;
}                       

Matrix &Matrix::operator/=(float d)
{
	unsigned i,j;
	for (i=0;i<Height; i++)
	{
		for (j=0;j<Width;j++)
		{
			Values[i*W16+j]/=d;
		}
	}
	return *this;
}                           

Matrix operator*(const Matrix &M,float d)
{            
	unsigned i,j;
	Matrix temp(M.Height,M.Width);

	for (i=0;i<M.Height; i++)
	{
		for (j=0;j<M.Width;j++)
		{
			temp.Values[i*temp.W16+j]=M.Values[i*M.W16+j]*d;
		}
	}
	return temp;
}

// Partial pivoting function
    
void Matrix::SwapRows(unsigned R1,unsigned R2)
{
	unsigned i;
	for(i=0;i<Width;i++)
	{
		float temp=operator()(R1,i);
		operator()(R1,i)=operator()(R2,i);  
		operator()(R2,i)=temp;
	}
}                      

float &Matrix::operator()(unsigned row,unsigned col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=Height||col>=Width||row<0||col<0)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw BadSize();
	}
#endif           
	return Values[row*W16+col];
}                  

float Matrix::operator()(unsigned row,unsigned col) const
{
#ifdef CHECK_MATRIX_BOUNDS
	if(row>=Height||col>=Width||row<0||col<0)
		throw BadSize();
#endif
	return Values[row*W16+col];
}

unsigned Matrix::Pivot(unsigned row,unsigned sz)
{
	unsigned k=row;
	float amax,temp;

	amax=-1;
	for(unsigned i=row;i<sz;i++)
	{
		if((temp=Fabs(Values[i*W16+row]))>amax&&temp!=0.0)
		{
			amax=temp;
			k=i;
		}
	}
	if(Values[k*W16+row]==0)
	{
		return 0xFFFFFFFF;
	}
	if(k!=unsigned(row))
	{                          
		SwapRows(k,row);
//		float *rowptr=&Values[k];
//		Values[k]=Values[row];
//		Values[row]=&rowptr;
		return k;
	}
	return 0;
}

Vector Matrix::Solve(const Vector& v)
{         
	Vector solution(v.size);
	unsigned i,j,k;
	float diagonal;
#ifdef CHECK_MATRIX_BOUNDS
	if(Height!=Width||Width!=v.size)
		throw Matrix::BadSize();
#endif
	Matrix temp(Height,Width+1);
	for (i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			temp.Values[i*temp.W16+j]=Values[i*W16+j];
		}
		temp.Values[i*temp.W16+Width]=v(i);
	}
	for(k=0;k<Height;k++)
	{
		unsigned index=temp.Pivot(k,Height);
		if(index==-1)
		{
			solution.Zero();
			return solution;
//			throw Singular();
		}
		diagonal=temp.Values[k*temp.W16+k];
		if(diagonal!=1)
		{
			for(j=k;j<temp.Width;j++)
			{
				temp(k,j)/=diagonal;
			}
		}
		for(i=k+1;i<Height;i++)
		{
			diagonal=temp(i,k);
			if(diagonal!=0)
			{
				for(j=k;j<temp.Width;j++)
				{
					temp.Values[i*temp.W16+j]-=diagonal*temp(k,j);
				}
			}
		}
	}
	for(int M=int(Height)-1;M>=0;M--)
	{
		solution[M]=temp.Values[M*temp.W16+Width];
		for(j=M+1;j<Width;j++)
			solution[M]-=temp.Values[M*temp.W16+j]*solution[j];
	}
	return solution;
}

// calculate the determinant of a matrix

float Matrix::Det(void)
{
	Matrix DetTemp;
	unsigned i,j,k;
	float piv;
	float detVal=1;

#ifdef CHECK_MATRIX_BOUNDS
	if(Height!=Width||Height>40)
		throw Matrix::BadSize();
#endif
	DetTemp.Resize(Height,Height);
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]=Values[i*W16+j];
		}
	}
	for(k=0;k<Height;k++)
	{
		unsigned indx=DetTemp.Pivot(k,Height);   // Find the biggest value in column k, then swap rows so it's at row k as well.
		if(indx==-1)
			return 0;
		if(indx!=0)
			detVal=-detVal;
		detVal=detVal*DetTemp.Values[k*DetTemp.W16+k];
		for(i=k+1;i<Height;i++)
		{
			piv=DetTemp(i,k)/DetTemp.Values[k*DetTemp.W16+k];
			for(j=k+1;j<Height;j++)
				DetTemp.Values[i*DetTemp.W16+j]-=piv*DetTemp(k,j);
		}
	}
	return detVal;
}

float Matrix::DetNoPivot(void)
{
	Matrix DetTemp;
	unsigned i,j,k;
	float piv;
	float detVal=1;
#ifdef CHECK_MATRIX_BOUNDS
	if(Height!=Width||Height>100)
		throw Matrix::BadSize();
#endif
	DetTemp.Resize(Height,Height);
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]=Values[i*W16+j];
		}
	}
	for(i=0;i<Height;i++)
	{
		detVal=detVal*DetTemp.Values[i*DetTemp.W16+i];
		for(j=i+1;j<Height;j++)
		{
			piv=DetTemp(j,i)/DetTemp.Values[i*DetTemp.W16+i];
			for(k=i;k<Height;k++)			// was i+1
				DetTemp.Values[j*DetTemp.W16+k]-=piv*DetTemp(i,k);
		}
	}
	return detVal;
}

float Matrix::SubDet(unsigned h)
{
	if(!h)
    	return 1.f;
	Matrix DetTemp;
	unsigned i,j,k;
	float piv;
	float detVal=1;
	DetTemp.Resize(h,h);
	for(i=0;i<h;i++)
	{
		for(j=0;j<h;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]=Values[i*W16+j];
		}
	}
	for(k=0;k<h;k++)
	{
		unsigned indx=DetTemp.Pivot(k,h);
		if(indx==-1)
			return 0;
		if(indx!=0)
			detVal=-detVal;
		detVal=detVal*DetTemp.Values[k*DetTemp.W16+k];
		for(i=k+1;i<h;i++)
		{
			piv=DetTemp(i,k)/DetTemp.Values[k*DetTemp.W16+k];
			for(j=k+1;j<h;j++)
				DetTemp.Values[i*DetTemp.W16+j]-=piv*DetTemp(k,j);
		}
	}
	return detVal;
}

float Matrix::CPlusPlusSubDetNoPivot(unsigned size,Matrix &Mul2)
{
	Matrix DetTemp;
	unsigned i,j,k;
	float detVal=1;
#ifdef CHECK_MATRIX_BOUNDS
	if(size>100)
		throw Matrix::BadSize();
#endif
	DetTemp.Resize(size,size);
	if(size==0)
		return 1.f;	 
	float piv;
	for(i=0;i<size;i++)
	{
		for(j=0;j<size;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]=Values[i*W16+j];
		}
	}
	for(i=0;i<size;i++)
	{
		detVal*=DetTemp.Values[i*DetTemp.W16+i];
		if(detVal==0)
			return 0;
		for(j=i+1;j<size;j++)
		{
			piv=DetTemp.Values[j*DetTemp.W16+i]/DetTemp.Values[i*DetTemp.W16+i];
			Mul2.Values[i*Mul2.W16+j]=piv;
			for(k=i;k<size;k++)
				DetTemp(j,k)-=piv*DetTemp(i,k);
		}
	}
	return detVal;
}

float Matrix::SubDetCholesky(unsigned size,Matrix &Fac)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(size!=Height)
    	throw Matrix::BadSize();  
	if(W16<size)
    	throw Matrix::BadSize();
#endif
	if(!CholeskyFactorization(Fac))
    	return 0.f;
	return DetFromCholeskyFactors(size,Fac);
}

float Matrix::SubDetNoPivot(unsigned sze,Matrix &Mul2)
{
	Matrix DetTemp;
	float detVal=1;
#ifdef CHECK_MATRIX_BOUNDS
	if(sze>100)
		throw Matrix::BadSize();
#endif
	if(sze==0)
		return 1.f;	      
	DetTemp.Resize(sze,sze);
#ifndef SIMD
#ifndef PLAYSTATION2
	float piv;    
	unsigned i,j,k;
	for(i=0;i<sze;i++)
	{
		for(j=0;j<sze;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]=Values[i*W16+j];
		}
	}
	for(i=0;i<sze;i++)
	{
		detVal*=DetTemp.Values[i*DetTemp.W16+i];
		if(detVal==0)
			return 0;
		for(j=i+1;j<sze;j++)
		{
			piv=DetTemp.Values[j*DetTemp.W16+i]/DetTemp.Values[i*DetTemp.W16+i];
			Mul2.Values[i*Mul2.W16+j]=piv;
			for(k=i;k<sze;k++)
				DetTemp(j,k)-=piv*DetTemp(i,k);
		}
	}
#else
	float *det=&detVal;
	unsigned SDStride=4*DetTemp.W16;
	unsigned Stride=4*W16;
	float *val=Values;
	float *Mval=DetTemp.Values;
	unsigned MulII=(unsigned)(Mul2.Values);
	unsigned MulIJ=NULL;
	float *IRowI=DetTemp.Values;
	float *JRowI=NULL;
	unsigned MulStride=4*Mul2.W16;
	asm __volatile__
	(
	"
		.set noreorder
		beqz %0,i_Escape
		nop
		add t4,zero,%7
		add t5,zero,%8
		add t0,zero,%0
		Row_Loop:
			add t1,zero,%0
			add t2,zero,t4
			add t3,zero,t5
			Col_Loop:
				lqc2 vf1,0(t2)
				sqc2 vf1,0(t3)
				addi t3,16
				addi t2,16
				addi t1,-4
			bgtz t1,Col_Loop
			nop
			add t4,%9
			add t5,%5
			addi t0,-1
		bgtz t0,Row_Loop
		nop
	Add t0,zero,zero
	bge t0,%0,i_Escape
	nop
	iLoop:            
		vsub vf1,vf0,vf0
		add t5,zero,%1
		add t2,zero,det
		lwc1 f5,0(t5)
		lwc1 f2,0(t2)
		mul.s f2,f2,f5
		swc1 f2,0(t2)

		//comiss vf3,vf1
		//je Escap
		//nop
		
		add t2,zero,%1
		add t2,%5
		add %4,zero,t2
		
		add t2,zero,%3
		addi t2,4
		add %2,zero,t2

		add t1,zero,t0
		addi t1,1
		bge t1,%0,j_Escape
		nop
		jLoop:
			add t6,zero,%4
			add t5,zero,%1
			lwc1 f2,0(t6)		// f2 = DetTemp(j,i)
			lwc1 f3,0(t5)		// f3 = DetTemp(i,i)
			div.s f2,f2,f3     	// piv = f2 /= DetTemp(i,i)
			add t2,zero,%2
			swc1 f2,0(t2)
			
			mfc1 t3,$f2
			ctc2 t3,$vi21							// Transfer t3's contents to the I register. And wait, because of latency...
			nop
			nop
			nop
			//shufps vf2,vf2,0		// vf2(0,1,2,3)=piv
			add t3,zero,t0				// k=i
			beq t3,%0,k_Escape
			nop
			kLoop:
				lwc1 f4,0(t5)	// f4 = DetTemp(i,k)
				mul.s f4,f2		// f4 = piv*DetTemp(i,k)
				lwc1 f5,0(t6)	// f5 = DetTemp(j,k)
				sub.s f5,f5,f4	// f5 -= piv * DetTemp(i,k)
				swc1 f5,0(t6)	//
				addi t5,4		// i,k+1
				addi t6,4		// j,k+1
				addi t3,1
				//cmp t3,%0
			blt t3,%0,kLoop
	nop
			k_Escape:
			add t2,zero,%4
			add t2,t2,%5
			add %4,zero,t2
			add t2,zero,%2
			addi t2,4
			add %2,zero,t2
			addi t1,1
			//cmp t1,%0
		blt t1,%0,jLoop
	nop
		j_Escape:

		add t2,zero,%1
		add t2,%5
		addi t2,4
		add %1,zero,t2

		add t2,zero,%3
		add t2,t2,%6
		addi t2,4
		add %3,zero,t2

		addi t0,1
//		cmp t0,%0
	blt t0,%0,iLoop
	nop
	i_Escape:
	Escap:
	"	:
		: "g"(sze),"g"(IRowI),"g"(MulIJ),"g"(MulII),"g"(JRowI),
			"g"(SDStride),"g"(MulStride),"g"(val),"g"(Mval),"g"(Stride)
		:  "t1","t2","t3","t4","t5","t6","t7","t8","t9",
			"$vf1","$f1","$f2"
	);
#endif
#else
	float *det=&detVal;
	unsigned SDStride=4*DetTemp.W16;
	unsigned Stride=4*W16;
	float *val=Values;
	float *Mval=DetTemp.Values;
	unsigned MulII=(unsigned)(Mul2.Values);
	unsigned MulIJ;
	float *IRowI=DetTemp.Values;
	float *JRowI;
	unsigned MulStride=4*Mul2.W16;
	__asm
	{
		mov esi,this
		mov esi,val
		mov edi,dword ptr[Mval]
		mov eax,sze
		Row_Loop:
			mov ebx,sze
			push esi
			push edi
			Col_Loop:
				movaps xmm1,[esi]
				movaps [edi],xmm1
				add edi,16
				add esi,16
				sub ebx,4
				cmp ebx,0
			jg Col_Loop
			pop edi
			pop esi
			add edi,SDStride
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop
	}           
	//float piv;   
	//float *Piv=&piv;
	//for(i=0;i<size;i++)
	{
	//	if(DetTemp.Values[i*DetTemp.W16+i]==0)
	//		return 0;
		//detVal*=DetTemp.Values[i*DetTemp.W16+i];
		//if(detVal==0)
		//	return 0;
		//(unsigned)JRowI=(unsigned)IRowI+SDStride;
		//MulIJ=MulII+4;
		//for(j=i+1;j<size;j++)
		//{
			//piv=DetTemp.Values[j*DetTemp.W16+i]/DetTemp.Values[i*DetTemp.W16+i];
			//Mul2.Values[i*Mul2.W16+j]=piv;
			//for(k=i;k<size;k++)
			//	DetTemp(j,k)-=piv*DetTemp(i,k);
		__asm
		{
		mov eax,0
		cmp eax,sze
		jge i_Escape
		iLoop:            
			xorps xmm0,xmm0
			mov edi,IRowI
			mov edx,det
			movss xmm6,[edi]
			mulss xmm6,[edx]
			movss [edx],xmm6

			comiss xmm6,xmm0
			je Escap

			mov edx,IRowI
			add edx,SDStride
			mov JRowI,edx
			
			mov edx,MulII
			add edx,4
			mov MulIJ,edx

			mov ebx,eax
			inc ebx
			cmp ebx,sze
			jge j_Escape
			jLoop:
				mov esi,JRowI
				mov edi,IRowI
				movss xmm5,[esi]		// xmm5 = DetTemp(j,i)
				movss xmm6,[edi]		// xmm5 = DetTemp(i,i)
				divss xmm5,xmm6     	// piv=xmm5 /= DetTemp(i,i)
				mov edx,MulIJ
				movss [edx],xmm5
				shufps xmm5,xmm5,0		// xmm5(0,1,2,3)=piv
				mov ecx,eax				// k=i
				cmp ecx,sze
				je k_Escape
				kLoop:
					movss xmm4,[edi]	// xmm4 = DetTemp(i,k)
					mulss xmm4,xmm5		// xmm4 =piv*DetTemp(i,k)
					movss xmm3,[esi]	// xmm3 = DetTemp(j,k)
					subss xmm3,xmm4		// xmm3 -= piv * DetTemp(i,k)
					movss [esi],xmm3    //
					add edi,4			// i,k+1
					add esi,4			// j,k+1
					inc ecx
					cmp ecx,sze
				jl kLoop
				k_Escape:
				mov edx,JRowI
				add edx,SDStride
				mov JRowI,edx
				mov edx,MulIJ
				add edx,4
				mov MulIJ,edx
				inc ebx
				cmp ebx,sze
			jl jLoop
			j_Escape:

			mov edx,IRowI
			add edx,SDStride
			add edx,4
			mov IRowI,edx

			mov edx,MulII
			add edx,MulStride
			add edx,4
			mov MulII,edx

			inc eax
			cmp eax,sze
		jl iLoop
		i_Escape:
		Escap:
		}
			//(unsigned)JRowI+=SDStride;
			//MulIJ+=4;
		//}
//		(unsigned)IRowI+=SDStride+4;
//		(unsigned)MulII+=MulStride+4;
	}
/*
	_asm
	{
		mov esi,this
		mov esi,val
		mov edi,dword ptr[Mval]
		mov eax,size
		Row_Loop:
			mov ebx,size
			push esi
			push edi
			Col_Loop:
				movaps xmm1,[esi]
				movaps [edi],xmm1
				add edi,16
				add esi,16
				sub ebx,4
				cmp ebx,0
			jg Col_Loop
			pop edi
			pop esi
			add edi,SDStride
			add esi,Stride
			dec eax
			cmp eax,0
		jg Row_Loop

		xorps xmm7,xmm7
//	for(i=0;i<size;i++)
//	{
		mov edi,det
		mov ecx,0
		movss xmm0,[edi]
		iLoop:
			mov esi,IRowI         // DetTemp.Values[i,0];
			movss xmm6,[esi]   	//xmm6(0)=DetTemp(i,i)
			//pop esi
			mulss xmm0,xmm6  	//detVal*=xmm6(0)    
			mov edi,det         //
			movss [edi],xmm0    //
//		if(detVal==0)
//			return 0;
			cmp [edi],0
			je Escap
			!!!DONT USE RCP, iT's BAD! rcpss xmm6,xmm6		// xmm6 = 1/DetTemp(i,i)
			shufps xmm6,xmm6,0	// xmm6(0,1,2,3)=1/DetTemp(i,i)
			//mov esi,edi			// esi=&DetTemp.Values[i*DetTemp.W16+i]
		//push esi
			mov edi,MulII
//		for(j=i+1;j<size;j++)
//		{
			mov edx,ecx
			inc edx       
			cmp edx,size
			je j_Escape
			jLoop:
				add esi,SDStride       
				mov JRowI,esi
				add edi,4			 // Multipliers(i,j)
//			piv=DetTemp.Values[j,i]/DetTemp.Values[i,i];
				movss xmm5,[esi]	// xmm5 = DetTemp(j,i)
				mulss xmm5,xmm6     // piv=xmm5 /= DetTemp(i,i)
				shufps xmm5,xmm5,0	// xmm5(0,1,2,3)=piv
//			Mul2.Values[i*Mul2.W16+j]=piv;
				movss [edi],xmm5     
//			for(k=i+1;k<size;k++)
				mov eax,ecx			// k=i
				//inc eax				// k=i+1
				cmp eax,size
				je k_Escape
				push esi     
				push edi
				mov esi,JRowI
				mov edi,IRowI
				//add edi,4			// i,k+1
				//add esi,4			// j,k+1
				kLoop:
//				DetTemp(j,k)-=piv*DetTemp(i,k);
					movss xmm4,[edi]	// xmm4 = DetTemp(i,k)
					mulss xmm4,xmm5		// xmm4 =piv*DetTemp(i,k)
					movss xmm3,[esi]	// xmm3 = DetTemp(j,k)
					subss xmm3,xmm4		// xmm3 -= piv * DetTemp(i,k)
					movss [esi],xmm3    //
					add edi,4			// i,k+1
					add esi,4			// j,k+1
					inc eax
					cmp eax,size
				jl kLoop   
				pop edi
				pop esi
				k_Escape:
				inc edx
				cmp edx,size
//		}
			jl jLoop
			j_Escape:
			mov edi,MulII
			add edi,MulStride
			add edi,4
			mov MulII,edi         // Mul = Mul(i+1,i+1)

			mov esi,IRowI
			add esi,SDStride// DetTemp.Values[(i+1)*...  
			add esi,4  
			mov IRowI,esi
			inc ecx
			cmp ecx,size   
//	}
		jl iLoop
		Escap:
	}          */
#endif
	return detVal;
}

// Inverse of this matrix

void Matrix::Inverse(Matrix &Result)
{
	Matrix DetTemp;
	unsigned i,j,k;
	float a1,a2;
#ifdef CHECK_MATRIX_BOUNDS
	if(Height!=Width)
		throw Matrix::BadSize();
#endif
	DetTemp=*this;
	Result.Unit();
	for(i=0;i<Height;i++)
	{
		unsigned index=DetTemp.Pivot(i,Height);
#ifndef NDEBUG
#ifdef SIMUL_EXCEPTIONS
		if(index==-1)
			throw Singular();
#endif
#endif

		if(index!=0)
		{
			Result.SwapRows(i,index);
		}
		a1=DetTemp.Values[i*DetTemp.W16+i];
		for(j=0;j<Height;j++)
		{
			DetTemp.Values[i*DetTemp.W16+j]/=a1;
			Result.Values[i*Result.W16+j]/=a1;
		}
		for(j=0;j<Height;j++)
		{
			if(j!=i)
			{
				a2=DetTemp.Values[j*DetTemp.W16+i];
				for(k=0;k<Height;k++)
				{
					DetTemp(j,k)-=a2*DetTemp(i,k);
					Result(j,k)-=a2*Result(i,k);
				}
			}
		}
	}
}

bool Matrix::SymmetricInverseNoPivot(Matrix &Result)
{
	Matrix DetTemp;
	DetTemp.ResizeOnly(Height,Height);
	return SymmetricInverseNoPivot(Result,DetTemp);
}

bool Matrix::SymmetricInverseNoPivot(Matrix &Result,Matrix &Fac)
{             
	if(!CholeskyFactorization(Fac))
		return false;
	InverseFromCholeskyFactors(Result,Fac);
	return true;
}

// Assuming in advance that the matrix and its result will both be symmetric
// Cholesky decomposition is a special case of LU decomposition.

// Uses only lower half of matrix:

bool Matrix::CholeskyFactorizationFromRow(Matrix &Fac,unsigned Idx)
{
#ifndef SIMD
	unsigned i,j,k;
	float sum;
	Fac.Resize(Height,Height);
	for(i=Idx;i<Height;i++)
	{
		for(j=i;j<Height;j++)
		{
			sum=operator()(j,i);
			for(k=0;k<i;k++)
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
#else
	Fac.ResizeOnly(Height,Height);   
	unsigned i;      
//	float sum;
/*	for(i=0;i<Height;i++)
	{
		for(unsigned j=i;j<Height;j++)
		{
			sum=operator()(j,i);
			for(unsigned k=0;k<i;k++)
				sum-=Fac(i,k)*Fac(j,k);
			if(i==j)
            {
				if(sum<0)
                {
                	unsigned stophere=1;
                    stophere++;
					stophere;
                    return false;
                }
		   		sum=Sqrt(sum);
			}
			else
			   	sum/=Fac(i,i);
			Fac(j,i)=sum;
		}
	}       */
	unsigned H1=Height;
	unsigned TempStride=4*Fac.W16;
	unsigned StridePlus4=4*(W16+1);
	unsigned Stride=4*W16;
//	Idx=0;
	float *DT=&(Fac.Values[(Fac.W16+1)*Idx]);  
	float *Row=&(Fac.Values[(Fac.W16)*Idx]);
	bool Res=true;
	bool *R=&Res;
float a;//,b;
float *aa=&a;
	_asm
	{
		xorps xmm6,xmm6  
		xorps xmm5,xmm5
		mov eax,Idx
		cmp eax,H1
		je RetTru
		//mov edi,DT
		mov esi,DT
		//mov edx,[edi]
		mov edi,Row
		Decomposei:
			mov ebx,eax                	// j=i;
			mov i,eax
			push eax
			mul StridePlus4
			mov edx,this
			mov edx,[edx]
			add eax,edx					// &Values(i,0)
			mov edx,edi
			push esi
			Decomposej:
				push edi
				push edx
				mov ecx,this
				mov ecx,[ecx]
				xorps xmm1,xmm1
				movss xmm1,[eax]        // Values(i,j)
			//movss b,xmm1
				mov ecx,i				// k=0;
				cmp ecx,0
				je kEscape     
				cmp ecx,4
				jl kTail
				Decomposek:
					movaps xmm2,[edi]   // edi=&Fac(i,k)
					mulps xmm2,[edx]	// edx=&Fac(j,k)
					subps xmm1,xmm2		// xmm1=Values(i,j)-sumX,-sumY,-sumZ,-sumW
					add edi,16
					add edx,16
					add ecx,-4			// k++;
					cmp ecx,3
				jg Decomposek
				kTail:
				cmp ecx,0
				je kAdd4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				cmp ecx,1
				je kAdd4       
					add edi,4
					add edx,4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				cmp ecx,2
				je kAdd4         
					add edi,4
					add edx,4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				kAdd4:
				// Now add the four sums together:
				movhlps xmm2,xmm1
				addps xmm1,xmm2         // X+Z,Y+W,--,--
				movaps xmm2,xmm1
				shufps xmm2,xmm2,1      // Y+W,--,--,--
				addss xmm1,xmm2
				kEscape:
				cmp ebx,i
				jne nosqrt
					mov edx,aa
					movss [edx],xmm5
					movss [edx],xmm1
					movss xmm2,xmm1
					cmpltss xmm2,xmm5
					movss [edx],xmm2
					mov edx,[edx]
					cmp edx,0
					jne RetFalse
					sqrtss xmm1,xmm1
					movss xmm3,xmm1		// Values(i,i)
					jmp common
				nosqrt:  
			//movss b,xmm1
			//movss b,xmm3
					divss xmm1,xmm3
				common:
			//movss b,xmm1
				movss [esi],xmm1
				pop edx
				pop edi
				add edx,TempStride
				add esi,TempStride
				add ebx,1               // j++
				add eax,Stride
				cmp ebx,H1
			jl Decomposej
			pop esi
	        add esi,4       
			add esi,TempStride
	        add edi,TempStride
			pop eax
			add eax,1
			cmp eax,H1
		jl Decomposei
		jmp RetTru
		RetFalse:
		xor eax,eax  
		mov ebx,R
		mov [ebx],eax
		RetTru:
	}
	return Res;
#endif
}

bool Matrix::CholeskyFactorization(Matrix &Fac)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i,j,k;
	float sum;
	Fac.Resize(Height,Height);
	//Fac.Zero();
	for(i=0;i<Height;i++)
	{
		for(j=i;j<Height;j++)
		{
			sum=operator()(j,i);
			for(k=0;k<i;k++)
				sum-=Fac(i,k)*Fac(j,k);
			if(i==j)
            {
				if(sum<0)
                {
                    return false;
                }
		   		sum= sqrt(sum);
			}
			else
			   	sum/=Fac(i,i);
			Fac(j,i)=sum;
		}
	}
	return true;
#else
	Fac.ResizeOnly(Height,Height);
// Cholesky:
	//float sum;
// Decompose:
	//Fac.Zero();
	/*if(Height==1)
	{
		asm __volatile__
		(
		"
			.set noreorder
			lwc1 $f1,0(%0)
			sqrt.s $f2,$f1
			swc1 $f2,0(%1)
			.set reorder
		"	:
			: "g"(Values),"g"(Fac.Values)
			:
		);
		return true;
	}*/
	unsigned Stride=W16*4;
	unsigned TempStride=Fac.W16*4;
	/*if(Height==2)
	{
		asm __volatile__
		(
		"
			.set noreorder
			lwc1 $f0,0(%0)
			sqrt.s $f0,$f0
			swc1 $f0,0(%1)
			
			add %0,%2
			lwc1 $f1,0(%0)
			div.s $f1,$f1,$f0
			add %1,%3
			swc1 $f1,0(%1)
			
			lwc1 $f11,4(%0)
			mul.s $f4,$f1,$f1
			sub.s $f11,$f11,$f4
			sqrt.s $f11,$f11
			swc1 $f11,4(%1)
		
			b RetTrue2
			nop
			RetFalse2:
			xor v0,v0
			RetTrue2:
			add v0,zero,1
			.set reorder
		"	:
			: "g"(Values),"g"(Fac.Values),"g"(Stride),"g"(TempStride)
			:
		);
		return true;
	}
	if(Height==3)
	{
		asm __volatile__
		(
		"
			.set noreorder
			lw t0,0(%0)
			lw s0,0(%1)
			add t8,zero,%2
			add t1,t0,t8
			add t2,t1,t8
			add t8,zero,%3
			add s1,s0,t8
			add s2,s1,t8
			
			lwc1 $f1,0(t0)
			sqrt.s $f2,$f1
			swc1 $f2,0(s0)
			
			lwc1 $f3,0(t1)
			div.s $f3,$f3,$f2
			swc1 $f3,0(s1)
			
			lwc1 $f4,0(t2)
			div.s $f4,$f4,$f2
			swc1 $f4,0(s2)
			
			lwc1 $f5,4(t1)
			lwc1 $f10,0(s1)
			mul.s $f11,$f10,$f10
			sub.s $f5,$f5,$f11
			sqrt.s $f5,$f5
			swc1 $f5,4(s1)
			
			lwc1 $f6,4(t2)
			lwc1 $f10,0(s1)
			lwc1 $f11,0(s2)
			mul.s $f12,$f10,$f11
			sub.s $f6,$f6,$f12
			div.s $f6,$f6,$f5
			swc1 $f6,4(s2)
			
			lwc1 $f7,8(t2)
			lwc1 $f10,0(s2)
			lwc1 $f11,4(s2)
			mul.s $f12,$f10,$f10
			mul.s $f13,$f11,$f11
			sub.s $f7,$f7,$f12
			sub.s $f7,$f7,$f13
			sqrt.s $f7,$f7
			swc1 $f7,8(s2)
			.set reorder
		"	:
			: "g"(this),"g"(&Fac),"g"(Stride),"g"(TempStride)
			: "s0","s1","s2","s3"
		);
		return true;
	}*/
	/*for(i=0;i<Height;i++)
	{
		for(j=i;j<Height;j++)
		{
			sum=operator()(j,i);
			for(k=0;k<i;k++)
				sum-=Fac(i,k)*Fac(j,k);
			if(i==j)
			{
				if(sum<=0)
					return false;
				sum=Sqrt(sum);
			}
			else
				sum/=Fac(i,i);
			Fac(j,i)=sum;
		}
	}*/
	asm __volatile__
	(
	"
		.set noreorder
		lw t0,0(%2)					// Values
		lw t1,0(%3)					// Fac.Values
		lw t4,12(%2)				// Height
		lw t5,16(%2)				// Width
		lw s0,0(%3)					// Fac(i,0)
		blez t4,iEscape0
		blez t5,iEscape0
		xor t6,t6					// i=0
		iLoop0:
			add t2,zero,t0			// Values(i,i)
			add t3,zero,t1			// Fac(i,i)
			add t7,zero,t6			// j=i
			add s1,zero,s0			// Fac(j,0)
			jLoop0:
				and t9,t2,~0xF		// 4-align the pointer
				lqc2 vf1,0(t9)		// load Values (i,j) aligned on 16 byte boundary

				and t8,t7,3
				beq t8,3,esccase	// W
				nop
				beq t8,0,esccase	// X
					vmr32 vf1,vf1
				beq t8,1,esccase	// Y
					vmr32 vf1,vf1
					vmr32 vf1,vf1	// Z
				esccase:
				vmulax ACC,vf0,vf0x	// ACC=0,0,0,0
				
				add s2,zero,s0
				add s3,zero,s1
				xor t8,t8			// k=0
				and s4,t6,0xFFFFFFFC// 4-align t6 into s4
				beqz t6,NoTail
				nop
				ble t6,3,kEscape0
				nop
				kLoop0:
					addi t8,4
					lqc2 vf2,0(s2)
					lqc2 vf3,0(s3)
					vmsuba ACC,vf2,vf3
					addi s2,16
				blt t8,s4,kLoop0
					addi s3,16
// Now ACC contains 4 sums to be added together.
				kEscape0:
				vmadd vf1,vf1,vf0	//ACC+0,0,0,Values(j,i)
				vaddx vf5,vf1,vf1x
				vaddy vf5,vf1,vf5y
				vaddz vf1,vf1,vf5z
				sub t8,t6,t8
				beqz t8,NoTail
				lqc2 vf2,0(s2)
				lqc2 vf3,0(s3)		// All of next 4 values
				vmul vf2,vf2,vf3
				vsubx vf1,vf1,vf2x	// w of vf1+ x of vf2
				beq t8,1,NoTail
				nop
				vsuby vf1,vf1,vf2y	// w of vf1+ y of vf2
				beq t8,2,NoTail
				nop
				vsubz vf1,vf1,vf2z	// w of vf1+ z of vf2
			NoTail:
				vmul vf1,vf1,vf0
				bne t6,t7,offdiag
				nop
				vsqrt Q,vf1w
				vwaitq
				vmulq vf1,vf0,Q
				vmove vf9,vf1
				vdiv Q,vf0w,vf1w
				b store
				nop
				offdiag:
				vdiv Q,vf0w,vf9w
				vwaitq
				vmulq vf1,vf1,Q
				store:

				and t9,t3,~0xF		// 4-align the pointer

				and t8,t6,3
				beq t8,3,esccase0a	// W
				nop
					vmr32 vf1,vf1
				beq t8,2,esccase0a	// Z
				nop
					vmr32 vf1,vf1
				beq t8,1,esccase0a	// Y
				nop
					vmr32 vf1,vf1	// X
				b emptycase
				nop
				esccase0a:
				lqc2 vf2,0(t9)		// load Fac(i,j) aligned on 16 byte boundary
				vadd vf1,vf1,vf2
				emptycase:
				sqc2 vf1,0(t9)
				
				addi t2,4			// Values(i,j+1)
				add t3,t3,%1		// Fac(j+1,i)
				add s1,s1,%1		// Fac(j+1,0)
				addi t7,1
				blt t7,t5,jLoop0
			nop
			add t0,t0,%0			// Values(i+1,i)
			add t1,t1,%1			// Fac(i+1,i)
			addi t0,4				// Values(i+1,i+1)
			addi t1,4				// Fac(i+1,i+1)
			add s0,s0,%1			// Fac(i+1,0)
			addi t6,1
			blt t6,t4,iLoop0
		nop
		iEscape0:
		.set reorder
	"	:
		: "g"(Stride),"g"(TempStride),"g"(this),"g"(&Fac)
		: "s0","s1","s2","s3","s4"
	);
	unsigned i,j;//,k;
	//float sum;
	for(i=0;i<Height;i++)
	{
		for(j=i+1;j<Height;j++)
		{
			if(Fac(i,j)!=0)
			{
				unsigned errorhere=1;
				errorhere++;
				errorhere;
				Fac(i,j)=0;
			}
		}
	}
	return true;
#endif
#else
	Fac.ResizeOnly(Height,Height);
	unsigned i;
	unsigned H1=Height;
	unsigned TempStride=4*Fac.W16;
	unsigned StridePlus4=4*(W16+1);  
	unsigned Stride=4*W16;
	Matrix *DT=&Fac;
//	float sum;
	_asm
	{
		xorps xmm6,xmm6
		mov eax,0
        mov edi,DT
		mov esi,[edi]
		mov edx,[edi]
		mov edi,[edi]
		Decomposei:
			mov ebx,eax                	// j=i;
			mov i,eax
			push eax
			mul StridePlus4
			mov edx,this
            mov edx,[edx]
			add eax,edx                 // &Values(i,0)
			mov edx,edi
			push esi
			Decomposej:
				push edi
            	push edx
				mov ecx,this
                mov ecx,[ecx]
				xorps xmm1,xmm1
                movss xmm1,[eax]        // Values(i,j)
				mov ecx,i				// k=0;
				cmp ecx,0
				je kEscape    
				cmp ecx,4
				jl kTail
				Decomposek:
					movaps xmm2,[edi]   // edi=&Fac(i,k)
					mulps xmm2,[edx]	// edx=&Fac(j,k)
					subps xmm1,xmm2		// xmm1=Values(i,j)-sumX,-sumY,-sumZ,-sumW
					add edi,16
					add edx,16
					add ecx,-4			// k++;
					cmp ecx,3
				jg Decomposek
				kTail:
				cmp ecx,0
				je kAdd4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				cmp ecx,1
				je kAdd4        
					add edi,4
					add edx,4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				cmp ecx,2
				je kAdd4        
					add edi,4
					add edx,4
					movss xmm2,[edi]
					mulss xmm2,[edx]
					subss xmm1,xmm2
				kAdd4:
				// Now add the four sums together:
				movhlps xmm2,xmm1
				addps xmm1,xmm2         // X+Z,Y+W,--,--
				movaps xmm2,xmm1
				shufps xmm2,xmm2,1      // Y+W,--,--,--
				addss xmm1,xmm2
				kEscape:
				cmp ebx,i
				jne nosqrt
					sqrtss xmm1,xmm1
					movss xmm3,xmm1		// Values(i,i)
					jmp common
				nosqrt:
					divss xmm1,xmm3
				common:
				movss [esi],xmm1
				pop edx
				pop edi
				add edx,TempStride
				add esi,TempStride
				add ebx,1               // j++
				add eax,Stride
				cmp ebx,H1
			jl Decomposej
			pop esi
	        add esi,4       
			add esi,TempStride
	        add edi,TempStride
			pop eax
			add eax,1
			cmp eax,H1
        jl Decomposei
	}
	return true;
#endif
}

float Matrix::DetFromCholeskyFactors(unsigned size,Matrix &Fac)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	float sum;
	float Result=1.f;
	for(unsigned i=0;i<size;i++)
	{
		sum=Fac(i,i);
		Result*=sum*sum;
	}
	return Result;
#else
	float Result=1.f;
	asm __volatile__
	(
	"
		.set noreorder
// All the following nonsense just to put 1.0 into fpu f1:
		//addi t1,zero,1
		//mtc1 t1,$f3
		//cvt.s.w $f1,$f3
		lwc1 $f1,0(%3)
		add t0,zero,%0
		muli t0,t0,4
		addi t0,4
		add t1,zero,%1
		add t2,zero,%2
		Loop:
			lwc1 $f2,0(t1)
			mul.s $f3,$f2,$f2
			mul.s $f1,$f1,$f3
			addi t2,-1
		bgtz t2,Loop
			add t1,t0
		swc1 $f1,0(%3)
		.set reorder
	"	:
		: "g"(Fac.W16),"g"(Fac.Values),"g"(size),"g"(&Result)
		:
	);
	return Result;
#endif
#else
	float sum;
	float Result=1.f;
	for(unsigned i=0;i<size;i++)
	{
		sum=Fac(i,i);
		Result*=sum*sum;
	}
	return Result;
#endif
}

void Matrix::InverseFromCholeskyFactors(Matrix &Result,Matrix &Fac)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	unsigned i,j,k;
// Cholesky:
	float sum;
//Invert:
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Height;j++)
		{
			sum=0;
			if(i==j)
				sum=1.f;
			for(k=0;k<j;k++)
				sum-=Fac(j,k)*Result(i,k);
            sum/=Fac(j,j);
			Result(i,j)=sum;
        }
		for(j=Height-1;j>=i;j--)
		{
			sum=Result(i,j);
			for(k=j+1;k<Height;k++)
				sum-=Fac(k,j)*Result(i,k);
			sum/=Fac(j,j);
			Result(i,j)=sum;
		}
	}
	for(i=0;i<Height;i++)
	{
		for(j=i+1;j<Height;j++)
		{
			Result(j,i)=Result(i,j);
		}
	}
#else
	//unsigned Stride=W16*4;
	unsigned TempStride=Fac.W16*4;
	unsigned RStride=Result.W16*4;
//Invert:
/*float sum;
    for(unsigned i=0;i<Height;i++)
	{
		for(unsigned j=0;j<Height;j++)
		{
			sum=0;
			if(i==j)
				sum=1;
			for(unsigned k=0;k<j;k++)
				sum-=Fac(j,k)*Result(i,k);
			sum/=Fac(j,j);
			Result(i,j)=sum;
		}
		for(unsigned j=Height-1;j>=i;j--)
		{
			sum=Result(i,j);
			for(unsigned k=j+1;k<Height;k++)
				sum-=Fac(k,j)*Result(i,k);
			sum/=Fac(j,j);
			Result(i,j)=sum;
		}
	}  */   
	if(Height==1)
	{
		Result.Values[0]=1.f/Fac.Values[0];
		return;
	}
	if(Height==2)
	{
		/*float sum;
		Result(0,0)=1.f/Fac(0,0);
		
		sum=-Fac(1,0)*Result(0,0);
		sum/=Fac(1,1);
		//Result(0,1)=sum;
		
		//sum=Result(0,1);
		sum/=Fac(1,1);
		Result(0,1)=sum;

		sum=Result(0,0);
		sum-=Fac(1,0)*Result(0,1);
		sum/=Fac(0,0);
		Result(0,0)=sum;

		//Result(1,0)=0.f;
		
		sum=1.f;
		sum/=Fac(1,1);
		//Result(1,1)=sum;
			
		//sum=Result(1,1);
		sum/=Fac(1,1);
		Result(1,1)=sum;

		Result(1,0)=Result(0,1);*/
		asm __volatile__
		(
		"
			.set noreorder
// All the following nonsense just to put 1.0 into fpu f1:
			addi t1,zero,1
			mtc1 t1,$f3
			cvt.s.w $f1,$f3
			
			lwc1 f0,0(%0)
			add t0,%2,%0
			lwc1 f10,0(t0)
			lwc1 f11,4(t0)
			
			div.s f2,f1,f0
			
			sub.s f4,f2,f2
			mul.s f3,f2,f10
			div.s f3,f3,f11
			div.s f3,f3,f11
			sub.s f4,f4,f3
			
			swc1 f4,4(%1)
			
			mul.s f5,f10,f4
			sub.s f5,f2,f5
			div.s f5,f5,f0
			swc1 f5,0(%1)
			
			add t1,%1,%3
			
			div.s f6,f1,f11
			div.s f6,f6,f11
			swc1 f6,4(t1)
			
			swc1 f4,0(t1)
			
			.set reorder
		"	:
			: "g"(Fac.Values),"g"(Result.Values),"g"(TempStride),"g"(RStride)
			:
		);
		return;
	}
	asm __volatile__
	(
	"
		.set noreorder
// All the following nonsense just to put 1.0 into fpu f1:
		addi t1,zero,1
		mtc1 t1,$f3
		cvt.s.w $f1,$f3
// All the following nonsense just to put 0.0 into fpu f0:
		xor t0,t0
		mtc1 t0,f0
		//cvt.s.w f0,f3
//-----------------------------------------------------------------
		lw t0,0(%0)					// Fac(i,0)
		lw t1,0(%1)					// Result(i,0)
		lw t9,12(%1)				// Height
		xor s0,s0
		iLoop1:
			lw t2,0(%0)				// Fac(j,0)
			lw t3,0(%0)				// Fac(j,j)
			add t7,zero,t1			// Result(i,j)
			xor s1,s1
			jLoop1:
				lwc1 f2,0(t3)		// Fac(j,j) 	
				div.s f2,f1,f2		// f2=1/f2
				mfc1 t8,f2			// t6=f2
				ctc2 t8,$vi21		// put 1/Fac(j,j) in the immediate I

				vsub vf1,vf0,vf0
				
				add t4,zero,t2		// Fac(j,k)
				add t5,zero,t1		// Result(i,k)
				beqz s1,NoTail
				xor s2,s2			// k=0
				ble s1,3,kEscape1
				and s4,s1,~0x3		// 4-align s1 into s4
				vsuba ACC,vf0,vf0	// ACC=0
				kLoop1:
					lqc2 vf2,0(t4)
					addi t4,16
					lqc2 vf3,0(t5)
					vmsuba ACC,vf2,vf3
					addi s2,4
				blt s2,s4,kLoop1
					addi t5,16
// Now vf1 contains 4 sums to be added together.
				vmaddx vf1,vf0,vf0x	//ACC+0,0,0,0
				vaddx vf5,vf1,vf1x
				vaddy vf5,vf5,vf1y
				vaddz vf1,vf5,vf1z
			kEscape1:
				sub s2,s1,s2
				beqz s2,NoTail
				lqc2 vf2,0(t4)
				lqc2 vf3,0(t5)		// All of next 4 values
				vmul vf2,vf2,vf3
				beq s2,1,NoTail
				vsubx vf1,vf1,vf2x	// w of vf1+ x of vf2
				beq s2,2,NoTail
				vsuby vf1,vf1,vf2y	// w of vf1+ y of vf2
				vsubz vf1,vf1,vf2z	// w of vf1+ z of vf2
			NoTail:
				
				bne s0,s1,offdiaga
				nop
				
				vadd vf1,vf1,vf0
				
				offdiaga:
				vmul vf1,vf1,vf0	// sum in vf1.w
				vmuli vf1,vf1,I		// *1/Fac(j,j)
				
				and t4,t7,~0xF		// 4-align the Result(i,j) pointer
				and t5,s1,3
				beq t5,3,esccase1a	// W
				nop
				beq t5,2,esccase1a	// Z
					vmr32 vf1,vf1
				beq t5,1,esccase1a	// Y
					vmr32 vf1,vf1
				b emptycase1
					vmr32 vf1,vf1	// X
				esccase1a:
				lqc2 vf2,0(t4)		// load Result(i,j) aligned on 16 byte boundary
				vadd vf1,vf1,vf2
				emptycase1:
				sqc2 vf1,0(t4)
				
				add t2,%2
				add t3,%2			// Fac(j+1,j)
				addi t3,4			// Fac(j+1,j+1)
				addi s1,1
			blt s1,t9,jLoop1
				addi t7,4			// Result(i,j+1)
			lw t2,0(%0)				// Fac(0,j)
			lw t3,0(%0)				// Fac(j,j)
        	add s1,zero,t9			// j=Height
        	addi s1,-1
			add t7,zero,s1			// t7=Height-1
			muli t7,t7,4			// t7=(Height-1)*4
			add t7,t7,t1			// Result(i,j), j=Height-1
			add t5,%2,4				// t7=(TempStride+4)
			mul t5,t5,s1			// (Height-1)*(TempStride+4)
			add t3,t3,t5			// Fac(j,j)
			jLoop2:
				lwc1 f2,0(t7)		// sum=Result(i,j)
				suba.s f2,f0		// ACC=f2
				add s2,s1,1			// k=j+1
				
				add t4,%2,t3		// Fac(k,j)
				add t5,t7,4			// Result(i,k)
				
				bge s2,t9,kEscape2	// k<Height
				nop
				kLoop2:
					lwc1 f3,0(t4)	// Fac(k,j)
					lwc1 f4,0(t5)	// Result(i,k)
					msuba.s f3,f4
					//mul.s f5,f3,f4	// Fac(k,j)*Result(i,k)
					//sub.s f2,f2,f5	// sum-=Fac(k,j)*Result(i,k);
					add t4,%2
					addi s2,1		// k++
				blt s2,t9,kLoop2	// k<Height
					addi t5,4
				kEscape2:
				madd.s f2,f0,f0
				lwc1 f6,0(t3)		// Fac(j,j)
				div.s f2,f2,f6		// sum/=Fac(j,j);
				swc1 f2,0(t7)		// Result(i,j)=sum;
				sub t3,%2			// Fac(j-1,j)
				addi t3,-4			// Fac(j-1,j-1)
				addi t2,-4			// Fac(0,j-1)
				addi s1,-1			//j--
			bge s1,s0,jLoop2		//j>=i
				addi t7,-4			// Result(i,j-1)
			add t0,t0,%2			// Fac(i+1,0)
			addi s0,1
		blt s0,t9,iLoop1
			add t1,t1,%3			// Result(i+1,0)
		xor s0,s0
		lw t0,0(%1)					// Result(i,i)
		iLoopSymmetry:
			add s1,zero,s0
			addi s1,1
			add t1,zero,t0			// Result(i,j)
			addi t1,4				// Result(i,j)
			add t2,%3,t0			// Result(j,i)
			jLoopSymmetry:
				lwc1 f1,0(t1)
				swc1 f1,0(t2)
				addi t1,4			// Result(i,j)
				addi s1,1
			blt s1,t9,jLoopSymmetry
				add t2,%3			// Result(j,i)
			add t0,%3
			addi t0,4
			addi s0,1
		blt s0,t9,iLoopSymmetry
		nop
	"	:
		: "g"(&Fac),"g"(&Result),"g"(TempStride),"g"(RStride)
		: "s0","s1","s2","s3","s4","s5","s6","s7"
	);
	/*for(unsigned i=0;i<Height;i++)
	{
		for(unsigned j=i+1;j<Height;j++)
		{
			Result(j,i)=Result(i,j);
		}
	}*/
#endif
#else
	unsigned i;//,k;
	unsigned H1=Height;
	unsigned TempStride=4*Fac.W16;
	unsigned RStride=4*Result.W16;
	//unsigned Stride=4*W16;
	//unsigned StridePlus4=4*(W16+1);
//	float *T2;
//	float *R2;
   // static float temp[4];
	//float *tt=temp;
	Matrix *DT=&Fac;
	Matrix *R=&Result;
// Cholesky:
//	float sum;
	//float *Resultjj=NULL;
//Invert:
/*   for(i=0;i<Height;i++)
	{
		for(j=0;j<Height;j++)
		{
			sum=0;
            if(i==j)
				sum=1;
			for(k=0;k<j;k++)
				sum-=Fac(j,k)*Result(i,k);
            sum/=Fac(j,j);
			Result(i,j)=sum;
		}
		for(j=Height-1;j>=i;j--)
		{
			sum=Result(i,j);
			for(k=j+1;k<Height;k++)
				sum-=Fac(k,j)*Result(i,k);
			sum/=Fac(j,j);
			Result(i,j)=sum;
		}
	}    */
	float one[]={1,0,0,0};
	float *one1=one;
	float *Resulti0=NULL;
	float *DetTempjj=NULL;
	Result.Zero();
	_asm
	{
		mov eax,one1;
#ifdef BORLAND
		movups xmm5,[eax];
#else                       
		movups xmm5,[eax];
#endif
		mov eax,0
		mov esi,R
		mov esi,dword ptr [esi]					// Result(i,0)
		iLoop:
			mov i,eax
			mov edx,DT
			mov edx,[edx]				// Fac(j,0)
			mov Resulti0,esi			// Result(i,0)
			mov ebx,DT					// Result(i,0)
			mov ebx,[ebx]
			mov DetTempjj,ebx			// Result(i,0)
        	push esi					// Result(i,j)
			mov ebx,0
			jLoop1:
				mov edi,edx				// Fac(j,k)
				push edx   
//mov edx,tt
				cmp ebx,i
				jne Off_Diagonal
				movaps xmm1,xmm5
				jmp On_Diagonal
			Off_Diagonal:
				xorps xmm1,xmm1
			On_Diagonal:
//movups [edx],xmm1
				mov eax,Resulti0
				mov ecx,0
				cmp ecx,ebx
				jge kEscape1
//movups [edx],xmm1
				kLoop1:
					movaps xmm2,[edi]   // Fac(j,k)
//movups [edx],xmm2
					mulps xmm2,[eax]    // Result(i,k)
//movups [edx],xmm2
					subps xmm1,xmm2
//movups [edx],xmm1
					add edi,16			// Fac(j,k+4)
					add eax,16			// Result(i,k+4)
                	add ecx,4
					cmp ecx,ebx
				jl kLoop1
				kEscape1:
			// Now add the four sums together:
				movhlps xmm2,xmm1
				addps xmm1,xmm2         // = X+Z,Y+W,--,--
				movaps xmm2,xmm1
				shufps xmm2,xmm2,1      // Y+W,--,--,--
				addss xmm1,xmm2
				mov edi,DetTempjj
				movss xmm2,[edi]
				divss xmm1,xmm2
				movss [esi],xmm1
				add edi,TempStride
				add edi,4    
				add esi,4
				mov DetTempjj,edi
				pop edx
				add edx,TempStride		// Fac(j+1,0)
				add ebx,1				// j++
				cmp ebx,H1				// j<Height
			jl jLoop1
                      
			pop esi
			push esi
			mov edx,DT
			mov edx,[edx]				// Fac(0,0)
			mov eax,H1
			dec eax						// =j
			imul eax,4
			mov edx,Resulti0			// Result(i,0)  
			add edx,eax					// Result(i,j)
	
			mov ebx,DetTempjj			// Fac(j,j)
			sub ebx,TempStride
			add ebx,-4
			mov DetTempjj,ebx			// Fac(j,j)

			mov ebx,H1
			dec ebx						// ebx,Height-1
                      
			mov edi,ebx					// =j
			imul edi,4
			add esi,edi                 // Result(i,j)
			jLoop2:
				mov eax,ebx				// =j
				add eax,1				// =j+1
				mul TempStride		// (j+1)*TempStride
				mov edi,DT
				mov edi,[edi]			// Fac(0,0)
				add edi,eax             // Fac(k,0)   
				mov eax,ebx				// =j
				imul eax,4				// j*4  
				add edi,eax             // Fac(k,j)
			//mov edx,tt
				mov eax,ebx             // =j
				imul eax,4              // (j+1)*4
				add eax,Resulti0        // Result(i,j)
				movss xmm1,[eax]
			//movss sum,xmm1
				add eax,4               // Result(i,k)
				mov ecx,ebx
				inc ecx
				cmp ecx,H1
				jge kEscape2
				kLoop2:
					movss xmm2,[edi]	// Fac(k,j)
				//movss sum,xmm2
					mulss xmm2,[eax]    // Result(i,k)
				//movss sum,xmm2
					subss xmm1,xmm2
				//movss sum,xmm1
					add edi,TempStride
					add eax,4
					add ecx,1
					cmp ecx,H1
                jl kLoop2
				kEscape2:
				mov edi,DetTempjj
				movss xmm2,[edi]
		   //	movss sum,xmm2
				divss xmm1,xmm2
			//movss sum,xmm1
				movss [esi],xmm1
				sub edi,TempStride
				add edi,-4
				mov DetTempjj,edi
				add esi,-4				// Result(i,j-1)
				//add edx,-4				// Fac(0,j-1)

				add ebx,-1				// j--

				cmp ebx,i				// j>=i
			jge jLoop2

			pop esi
			add esi,RStride				// Result(i,0)
			mov eax,i
            add eax,1
			cmp eax,H1
		jl iLoop

		mov esi,R
		mov eax,[esi+12]
		dec eax
		jle SymmEscape
		mov edi,[esi]					// Result(i+1,i)
		iSymmLoop:
			mov edx,edi   
			add edx,4
			mov ecx,edi   
			add ecx,RStride
			mov ebx,eax
			jSymmLoop:
				mov esi,[edx]
				mov [ecx],esi
				add ecx,RStride
				add edx,4
				dec ebx
			jg jSymmLoop
			add edi,RStride
			add edi,4
			dec eax
		jg iSymmLoop
		SymmEscape:
	}
/*	for(i=0;i<Height;i++)
	{
		for(j=i+1;j<Height;j++)
		{
			Result(j,i)=Result(i,j);
		}
	}*/
#endif
}             

//Incremental Determinant:
// Say we have already got the determinant for the matrix of size-1.
// We now have a matrix size x size which is the same except for the extra
// row and column. we need to first adjust the extra column up to element size-1
// subtracting the correct multiples of each row.
// Then we have a row remaining from which we subtract the correct multiple of
// each row up to size-1.
// What we need is a list of what multiples of a row i have been added to row j
// and in what order.

float Matrix::SubDetIncrementCholesky(float &detVal,unsigned size,Matrix &Fac)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	unsigned i,k;
// Cholesky:
	float sum;
// Decompose:
	unsigned N=size-1;
	for(i=0;i<size;i++)
	{
		sum=operator()(N,i);
		for(k=0;k<i;k++)
			sum-=Fac(i,k)*Fac(N,k);
		if(i==N)
		{
			if(sum<=0)
				return 0;
			sum=sqrt(sum);
		}
		else
		   	sum/=Fac(i,i);
		Fac(N,i)=sum;
	}
// Determinant:
	sum=Fac(N,N);
	float ret=detVal*sum*sum;
	return ret;
#else       
//	unsigned i,k;
// Cholesky:
	float ret;
	float *rr=&ret;
// Decompose:
	unsigned N=size-1;
	/*float sum;
	for(i=0;i<size;i++)
	{
		sum=operator()(N,i);
		for(k=0;k<i;k++)
			sum-=Fac(i,k)*Fac(N,k);
		if(i==N)
		{
			if(sum<=0)
				return 0;
			sum=Sqrt(sum);
		}
		else
		   	sum/=Fac(i,i);
		Fac(N,i)=sum;
	}
	sum=Fac(N,N);
	ret=detVal*sum*sum;  */
	unsigned Stride=W16*4;
	unsigned FStride=Fac.W16*4;
	float *dv=&detVal;
//	float tmp;
	_asm
	{
		xorps xmm6,xmm6
		mov esi,this
		mov edi,dword ptr [esi]
		mov eax,N
		jl EscapeAll
		mov ebx,Stride
		mul ebx
		add edi,eax
		mov esi,Fac
		mov ecx,[esi]  
		mov eax,N        
		mov ebx,FStride
		mul ebx
		add ecx,eax				// F(N,0)    
		mov eax,0        
		mov edx,[esi]           // F(i,0)
		cmp eax,N
		je Escape1
		iLoop:
			movss xmm1,[edi]  
	//movss tmp,xmm1
			add edi,4
			mov ebx,eax
			push ecx
			push edx  
			cmp eax,0
			je kEscape1
			kLoop1:
				movss xmm2,[ecx]
	//movss tmp,xmm2
				mulss xmm2,[edx]
	//movss tmp,xmm2
				subss xmm1,xmm2
	//movss tmp,xmm1
				add ecx,4
				add edx,4
				dec ebx
			jg kLoop1
			kEscape1:
			movss xmm2,[edx]
	//movss tmp,xmm2
			divss xmm1,xmm2
	//movss tmp,xmm1
			movss [ecx],xmm1
			pop edx
			pop ecx
			add edx,FStride
			inc eax
			cmp eax,N
		jl iLoop
		Escape1:
			movss xmm1,[edi]  
	//movss tmp,xmm1
			add edi,4
			mov ebx,eax
			cmp eax,0
			je kEscape2
			kLoop2:
				movss xmm2,[ecx]   
	//movss tmp,xmm2
				mulss xmm2,[edx]
	//movss tmp,xmm2
				subss xmm1,xmm2   
	//movss tmp,xmm1
				add ecx,4
				add edx,4
				dec ebx
			jg kLoop2    
			kEscape2:
			comiss xmm1,xmm6
			jle ReturnZero
			sqrtss xmm1,xmm1
			movss [ecx],xmm1
			mov eax,dv
			movss xmm2,[eax]
	//movss tmp,xmm2
			mulss xmm2,xmm1    
	//movss tmp,xmm2
			mulss xmm2,xmm1   
	//movss tmp,xmm2
			mov eax,rr
			movss [eax],xmm2
			jmp EscapeAll
		ReturnZero:
			mov eax,rr
			movss [eax],xmm6
		EscapeAll:
	};
	return ret;
#endif
#else
	if(size==1)
	{
		Fac.Values[0]=sqrt(Values[0]);
		return Fac.Values[0];
	}
	else if(size==2)
	{
		Fac(1,0)=Values[W16]/Fac.Values[0];
		float val=Fac(1,0);
		val*=-val;
		val+=Values[W16+1];
		if(val<=0)
			return 0;
		val=Sqrt(val);
		Fac(1,1)=val;
// Determinant:
		return detVal*val*val;
	}
	else if (size==3)
	{
		/*float sum;
		sum=operator()(2,0);
		sum/=Fac(0,0);
		Fac(2,0)=sum;
		
		sum=operator()(2,1);
		sum-=Fac(1,0)*Fac(2,0);
		sum/=Fac(1,1);
		Fac(2,1)=sum;
		
		sum=operator()(2,2);
		sum-=Fac(2,0)*Fac(2,0);
		sum-=Fac(2,1)*Fac(2,1);
		if(sum<=0)
			return 0;
		sum=Sqrt(sum);
		Fac(2,2)=sum;
	return detVal*sum*sum;*/
		
		float ret;
		unsigned Stride=W16*4;
		unsigned TempStride=Fac.W16*4;
		asm __volatile__
		(
		"
			.set noreorder
			lw t0,0(%0)
			lw t1,0(%1)
			add t2,zero,%3
			muli t2,t2,2
			add t0,t0,t2		// Values(2,0)
			
			add t3,zero,%4
			muli t3,t3,2
			add t2,t1,%4		// Fac(1,0)
			add t4,t1,t3		// Fac(2,0)
			
			lwc1 $f1,0(t0)
			lwc1 $f2,0(t1)		// Fac(0,0)
			div.s $f1,$f1,$f2	// sum/=Fac(0,0)
			swc1 $f1,0(t4)		// Fac(2,0)=sum
			
			lwc1 $f1,4(t0)		// Values(2,1)
			lwc1 $f2,0(t2)		// Fac(1,0)
			lwc1 $f3,0(t4)		// Fac(2,0)
			mul.s $f2,$f2,$f3
			sub.s $f1,$f1,$f2
			lwc1 $f2,4(t2)		// Fac(1,1)
			div.s $f1,$f1,$f2	// sum/=Fac(1,1)
			swc1 $f1,4(t4)		// Fac(2,1)=sum
			
			lwc1 $f1,8(t0)		// Values(2,2)
			lwc1 $f2,0(t4)		// Fac(2,0)
			mul.s $f2,$f2,$f2
			sub.s $f1,$f1,$f2
			lwc1 $f2,4(t4)		// Fac(2,1)
			mul.s $f2,$f2,$f2
			sub.s $f1,$f1,$f2
			
			xor t0,t0,t0
			mtc1 t0,$f0
			c.lt.s $f1,$f0
			bc1t ReturnFalse
			nop

			sqrt.s $f1,$f1
			swc1 $f1,8(t4)		// Fac(2,1)=sum
			
			lwc1 $f2,0(%5)
			mul.s $f2,$f2,$f1
			mul.s $f2,$f2,$f1
			swc1 $f2,0(%6)
			b Escape
			ReturnFalse:
			add %6,zero,zero
			Escape:
			.set reorder
		"	:
			: "g"(this),"g"(&Fac),"g"(size),"g"(Stride),"g"(TempStride),"g"(&detVal),"g"(&ret)
			: "s0","s1","s2","s3","s4","s5","s6","s7"
		);
		
// Determinant:
	return ret;
	}
	/*unsigned i,k;
	float sum;
	unsigned N=size-1;
	for(i=0;i<size;i++)
	{
		sum=operator()(N,i);
		for(k=0;k<i;k++)
			sum-=Fac(i,k)*Fac(N,k);
		if(i==N)
		{
			if(sum<=0)
				return 0;
			sum=Sqrt(sum);
		}
		else
		   	sum/=Fac(i,i);
		Fac(N,i)=sum;
	}*/
//	sum=Fac(N,N);
//	return detVal*sum*sum;
	float ret;
	unsigned Stride=W16*4;
	unsigned TempStride=Fac.W16*4;
	asm __volatile__
	(
	"
		.set noreorder
// All the following nonsense just to put 1.0 into fpu f1:
		addi t1,zero,1
		mtc1 t1,$f3
		cvt.s.w $f1,$f3				
		
		lw t0,0(%2)					// Values
		lw t1,0(%3)					// DetTemp.Values
		add t4,zero,%4				// Height
		lw s0,0(%3)					// DetTemp(i,0)
		blez t4,iEscape0
		xor t6,t6					// i=0
		add t7,zero,t4			// t7=size
		addi t7,-1				// t7=j=N=size-1
		
		add t2,zero,t7			// t2=N
		mul t2,t2,Stride		// Stride*N
		add t2,t0				// Values(N,i)
		
		add t3,zero,t7			// t3=N
		mul t3,t3,%1			// Stride*N
		add t3,t1				// DetTemp(N,i)
		
		add s1,zero,t7			// t3=N
		mul s1,s1,%1			// Stride*N
		add s1,s0				// DetTemp(N,0)
		
		iLoop0:
			beq t6,t7,ondiag
			
			lwc1 f2,0(t1)		// DetTemp(i,i) 	
			div.s f2,f1,f2		// f2=1/f2
			mfc1 t8,f2			// t6=f2
			ctc2 t8,$vi21		// put 1/DetTemp(j,j) in the immediate I
			ondiag:

			and t9,t2,~0xF		// 4-align the pointer
			lqc2 vf1,0(t9)		// load Values (i,j) aligned on 16 byte boundary
			
			and t8,t6,3
			beq t8,3,esccase	// W
			nop
			beq t8,0,esccase	// X
				vmr32 vf1,vf1
			beq t8,1,esccase	// Y
				vmr32 vf1,vf1
			vmr32 vf1,vf1		// Z
			esccase:
			add s2,zero,s0
			add s3,zero,s1
			vmul vf1,vf1,vf0	// vf1=0,0,0,Values(j,i)
			beqz t6,NoTail
			ble t6,3,kEscape0
			add t8,zero,t6		// k=0
			kLoop0:
				lqc2 vf2,0(s2)
				lqc2 vf3,0(s3)
				vmul vf2,vf2,vf3
				vsub vf1,vf1,vf2
				addi s2,16
				addi t8,-4
			bgt t8,3,kLoop0
				addi s3,16
// Now vf1 contains 4 sums to be added together.
			vaddx vf5,vf1,vf1x
			vaddy vf5,vf1,vf5y
			vaddz vf1,vf1,vf5z
			kEscape0:
			beqz t8,NoTail
			lqc2 vf2,0(s2)
			lqc2 vf3,0(s3)		// All of next 4 values
			vmul vf2,vf2,vf3
			beq t8,1,NoTail
			vsubx vf1,vf1,vf2x	// w of vf1+ x of vf2
			beq t8,2,NoTail
			vsuby vf1,vf1,vf2y	// w of vf1+ y of vf2
			vsubz vf1,vf1,vf2z	// w of vf1+ z of vf2
		NoTail:
			vmul vf1,vf1,vf0
			bne t6,t7,offdiag
			nop
			vsqrt Q,vf1w
			vwaitq
			cfc2 t9,vi16
			and t9,t9,16
			bnez t9,RetZero
			nop
			vmulq vf1,vf0,Q
			b store
			nop
			offdiag:
			vmuli vf1,vf1,I
			store:
			and t9,t3,~0xF		// 4-align the pointer
			and t8,t6,3
			beq t8,3,esccase0a	// W
			nop
				vmr32 vf1,vf1
			beq t8,2,esccase0a	// Z
			nop
				vmr32 vf1,vf1
			beq t8,1,esccase0a	// Y
			nop
				vmr32 vf1,vf1	// X
			b emptycase
			nop
			esccase0a:
			lqc2 vf2,0(t9)		// load DetTemp(i,j) aligned on 16 byte boundary
			vadd vf1,vf1,vf2
			emptycase:
			sqc2 vf1,0(t9)
	
		add t0,t0,%0			// Values(i+1,i)
		add t1,t1,%1			// DetTemp(i+1,i)
		addi t0,4				// Values(i+1,i+1)
		addi t1,4				// DetTemp(i+1,i+1)
		add s0,s0,%1			// DetTemp(i+1,0)
		
		addi t2,4				// Values(N,i)
		addi t3,4				// DetTemp(N,i)
		
		addi t6,1
		blt t6,t4,iLoop0
		nop
	sub t1,t1,%1			// DetTemp(i+1,i)
	addi t1,-4				// DetTemp(i,i)
	lwc1 f1,0(t1)
	add t5,zero,%5
	lwc1 f2,0(t5)
	mul.s f3,f1,f1
	mul.s f2,f2,f3
	swc1 f2,0(%6)
	b iEscape0
	RetZero:
	sub %6,%6,%6
	iEscape0:
	.set reorder
	"	:
		: "g"(Stride),"g"(TempStride),"g"(this),"g"(&Fac),"g"(size),"g"(&detVal),"g"(&ret)
		: "s0","s1","s2","s3","s4","s5","s6","s7"
	);
	return ret;
#endif
}                    

// Make smaller matrix from this one

/*Matrix Matrix::Reduced(unsigned rowcol)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(rowcol>=Width||rowcol>=Height||rowcol<0)
		throw Matrix::BadSize();
#endif
	Matrix M(Height-1,Width-1);
	unsigned i,j;
	for(i=0;i<Height-1;i++)
	{
		for(j=0;j<Width-1;j++)
		{
			M.Values[i*M.W16+j]=Values[i+(i>=rowcol)*W16+j+(j>=rowcol)];
		}
	}
	return M;
}*/

/*Matrix Matrix::RemoveRow(unsigned row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if (row>=Height||row<0)
		throw Matrix::BadSize();
#endif
	Matrix M(Height-1,Width);
	unsigned i,j;
	for(i=0;i<Height-1;i++)
	{
		for(j=0;j<Width;j++)
		{
			M.Values[i*M.W16+j]=Values[i+(i>=row)*W16+j];
		}
	}
	return M;
}        */

/*Matrix Matrix::RemoveColumn(unsigned col)
{                 
#ifdef CHECK_MATRIX_BOUNDS              
	if (col>=Width||col<0)
		throw Matrix::BadSize();
#endif
	Matrix M(Height,Width-1);
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width-1;j++)
		{
			M.Values[i*M.W16+j]=Values[i*W16+j+(j>=col)];
		}
	}
	return M;
}     */

float Matrix::CheckSum(void)
{
	float val=0;
	unsigned i,j;
	for(i=0;i<Height;i++)
	{
		for(j=0;j<Width;j++)
		{
			val+=Fabs(Values[i*W16+j]);
		}
	}
	return val;
}                             

bool Matrix::CheckNAN(void)
{                 
	unsigned i,j;
	for(i=0;i<Height;i++)//(unsigned)(((unsigned)Height+3)&~0x3)
	{
		for(j=0;j<Width;j++) ///(unsigned)(((unsigned)Width+3)&~0x3)
		{
#ifdef _MSC_VER
			if(_isnan(Values[i*W16+j]))
#else
			if(isnan(Values[i*W16+j]))
#endif         
			{
				return true;
			}
		}
	}
	return false;
}

void Matrix::InvertDiagonal(void)
{
	unsigned i;
	for(i=0;i<Height;i++)
	{
		Values[i*W16+i]=1/Values[i*W16+i];
	}
}

/*Matrix Matrix::HandleIndeterminacy(void)
{
// for each constraint, try x=1, apply the zero eigenvalue
// and try to get the eigenvector.
	unsigned i,j;
#ifdef CHECK_MATRIX_BOUNDS
	if(Height==1||Width!=Height)
		throw Matrix::BadSize();
#endif
	Matrix NewGamma(Height,Height);

	Matrix reduced(Height-1,Height-1);
	for (i=0;i<Height;i++)
	{
		Vector vr(Height-1);
		Vector ai(Height-1);
		for(j=0;j<Height-1;j++)
		{
			ai(j)=Values[j+(j>=i)*W16+i];  // ai is the i column of the matrix.
		}
		// strip out the i-th row and column from gamma
		reduced=Reduced(i);
		if(reduced.Det()<1e-5)
		{
			reduced=reduced.HandleIndeterminacy();
		}
		vr=reduced.Solve(-ai);
		for(j=0;j<Height-1;j++)
		{
			NewGamma.Values[i*NewGamma.W16+j+(j>=i)]=Values[i*W16+j+(j>=i)]+vr(j);
		}
		NewGamma.Values[i*NewGamma.W16+i]=Values[i*W16+i]+1;
	}         
	for (;i<Height;i++)
	{
		for(j=0;j<Height;j++)
		{
			NewGamma.Values[i*NewGamma.W16+j]=Values[i*W16+j];
		}
	}
	return NewGamma;
}              */

void Matrix::InsertVectorCrossMatrix(const Vector &v,const Matrix &M)
{
	unsigned i;
	for(i=0;i<Width;i++)
	{
		Values[0*W16+i]=-v(2)*M.Values[1*M.W16+i]+v(1)*M.Values[2*M.W16+i];
		Values[1*W16+i]=-v(0)*M.Values[2*M.W16+i]+v(2)*M.Values[0*M.W16+i];
		Values[2*W16+i]=-v(1)*M.Values[0*M.W16+i]+v(0)*M.Values[1*M.W16+i];
	}
}             

void Matrix::InsertAddVectorCrossMatrix(const Vector &v,const Matrix &M)
{
	unsigned i;
	for(i=0;i<Width;i++)
	{
		Values[0*W16+i]+=-v.Values[2]*M.Values[1*M.W16+i]+v.Values[1]*M.Values[2*M.W16+i];
		Values[1*W16+i]+=-v.Values[0]*M.Values[2*M.W16+i]+v.Values[2]*M.Values[0*M.W16+i];
		Values[2*W16+i]+=-v.Values[1]*M.Values[0*M.W16+i]+v.Values[0]*M.Values[1*M.W16+i];
	}
}

void Matrix::InsertSubtractVectorCrossMatrix(const Vector &v,const Matrix &M)
{
	unsigned i;
	for(i=0;i<Width;i++)
	{
		Values[0*W16+i]+=v(2)*M.Values[1*M.W16+i]-v(1)*M.Values[2*M.W16+i];
		Values[1*W16+i]+=v(0)*M.Values[2*M.W16+i]-v(2)*M.Values[0*M.W16+i];
		Values[2*W16+i]+=v(1)*M.Values[0*M.W16+i]-v(0)*M.Values[1*M.W16+i];
	}
}

void Matrix::OuterProduct(const Vector &v)
{
//	if(v.size!=3)
//		throw Matrix::BadSize();
//	Matrix M(v.size,v.size);
	unsigned i,j;
	for(i=0;i<v.size;i++)
	{
		for(j=0;j<v.size;j++)
		{
			Values[i*W16+j]=v(i)*v(j);
		}
	}
//	return M;
}

void SubtractTransposeTimesMatrix(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
#ifdef CHECK_MATRIX_BOUNDS
	if(M1.Height!=M2.Height||M.Height!=M1.Width||M.Width!=M2.Width)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif
	for(i=0;i<M1.Width;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			//float *m1row = &M1.Values[0*M.W16+i];
			float t=0.f;
			k = 0;
			unsigned l=M1.Height;
			while(l > 4)
			{
					t+=M1.Values[(k+0)+0*M1.W16+i]*M2.Values[(k+0)+0*M2.W16+j];
					t+=M1.Values[(k+1)+1*M1.W16+i]*M2.Values[(k+1)+1*M2.W16+j];
					t+=M1.Values[(k+2)+2*M1.W16+i]*M2.Values[(k+2)+2*M2.W16+j];
					t+=M1.Values[(k+3)+3*M1.W16+i]*M2.Values[(k+3)+3*M2.W16+j];
					k+=4;
					l-=4;
			}
			switch(l)
			{
				case 4:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;  // fall into 321 case
				case 3:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;
				case 2:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;
				case 1:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	break;
				case 0: break;
			}
			M.Values[i*M.W16+j]-=t;
		}
	}
}
// RUSSELL's APPROACH:
void MultiplyTransposeByMatrix(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
#ifdef CHECK_MATRIX_BOUNDS
	if(M1.Height!=M2.Height||M.Height!=M1.Width||M.Width!=M2.Width)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif
	for(i=0;i<M1.Width;i++)
	{
		for(j=0;j<M2.Width;j++)
		{
			//float *m1row=&M1.Values[0*M.W16+i];
			float t=0.f;
			k=0;
			unsigned l=M1.Height;
			while(l>4)
			{
					t+=M1.Values[(k+0)*M1.W16+i]*M2.Values[(k+0)*M2.W16+j];
					t+=M1.Values[(k+1)*M1.W16+i]*M2.Values[(k+1)*M2.W16+j];
					t+=M1.Values[(k+2)*M1.W16+i]*M2.Values[(k+2)*M2.W16+j];
					t+=M1.Values[(k+3)*M1.W16+i]*M2.Values[(k+3)*M2.W16+j];
					k+=4;
					l-=4;
			}
			switch(l)
			{
				case 4:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;  // fall into 321 case
				case 3:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;
				case 2:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	++k;
				case 1:	t+=M1.Values[k*M1.W16+i]*M2(k,j);	break;
				case 0: break;
			}
			M.Values[i*M.W16+j]=t;
		}
	}
}

void MultiplyByScalar(Matrix &result,const float f,const Matrix &M)
{
	unsigned i,j;
	for(i=0;i<M.Height;i++)
	{
		for(j=0;j<M.Width;j++)
		{
			result.Values[i*result.W16+j]=f*M.Values[i*M.W16+j];
		}
	}
}

void MultiplyAndAdd(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
	for(i=0;i<M1.Height;++i)
	{
		for(j=0;j<M2.Width;++j)
		{
			for(k=0;k<M1.Width;k++)
			{
				M.Values[i*M.W16+j]+=M1(i,k)*M2(k,j);
			}
		}
	}
}

void MultiplyAndSubtract(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
	for(i=0;i<M1.Height;++i)
	{
		for(j=0;j<M2.Width;j++)
		{
			for(k=0;k<M1.Width;k++)
			{
				M.Values[i*M.W16+j]-=M1(i,k)*M2(k,j);
			}
		}
	}
}

void MultiplyNegative(Matrix &M,const Matrix &M1,const Matrix &M2)
{
	unsigned i,j,k;
	for(i=0;i<M1.Height;++i)
	{
		for(j=0;j<M2.Width;++j)
		{             
			M.Values[i*M.W16+j]=0.f;
			for(k=0;k<M1.Width;k++)
			{
				M.Values[i*M.W16+j]-=M1(i,k)*M2(k,j);
			}
		}
	}
}

template<class T> static void Swap(T &t1,T &t2)
{
	T temp=t2;
	t2=t1;
	t1=temp;
}

void swap(Matrix &M1,Matrix &M2)
{
	Swap(M1.Width,M2.Width);
	Swap(M1.W16,M2.W16);
	Swap(M1.Height,M2.Height);
	Swap(M1.BufferSize,M2.BufferSize);
	Swap(M1.Values,M2.Values);
#ifdef CANNOT_ALIGN   
	Swap(M1.V,M2.V);
#endif
}
#ifndef SIMD
#ifndef PLAYSTATION2
void AddRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2)
{
	for(unsigned i=0;i<M1.Width;i++)
		M1(Row1,i)+=M2(Row2,i);
}
#else
void AddRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2)
{
	float *RowPtr1=&(M1.Values[Row1*M1.W16]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	//unsigned W=M1.Width;
	//unsigned W16=(M1.Width+3)&~0x3;
	//unsigned Stride1=M1.W16<<2;
	//unsigned Stride2=M2.W16<<2;
	asm __volatile__("
		.set noreorder
		lw t0,8(a0)				// t0= number of 4-wide columns
		add t1,zero,%0				// t1= address of matrix 1's row
		add t2,zero,%1
		blez t0,Escape				// for zero width matrix 
		iLoop:
			lqc2 vf1,0(t1)			// Load 4 numbers into vf1
			lqc2 vf2,0(t2)			// Load 4 numbers into vf2
			vadd vf1,vf1,vf2
			sqc2 vf1,0(t1)
			addi t1,16				// next four numbers
			addi t0,-4				// counter-=4
		bgtz t0,iLoop
			addi t2,16
		Escape:
	"					: 
						: "r" (RowPtr1), "r" (RowPtr2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
}
#endif
#else      
void AddRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2)
{
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	unsigned W=(M1.Width+3)&~0x3;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov esi,[ebx]
		mov eax,Row1
		mul Stride1
		add edi,eax
		mov eax,Row2
		mul Stride2
		add esi,eax
		mov eax,W		//[edx+16]
		shr eax,2
		iLoop:
			movaps xmm2,[esi]
			addps xmm2,[edi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
		dec eax
		cmp eax,0
		jne iLoop
	}
}
#endif

#ifndef SIMD
#ifndef PLAYSTATION2
void AddRowMultiple(Matrix &M1,unsigned Row1,float Multiplier,Matrix &M2,unsigned Row2)
{
	for(unsigned i=0;i<M1.Width;i++)
		M1(Row1,i)+=Multiplier*M2(Row2,i);
}
#else
void AddRowMultiple(Matrix &M1,unsigned Row1,float Multiplier,Matrix &M2,unsigned Row2)
{
	float d[4];
	d[0]=Multiplier;
	float *MM=&Multiplier;
	float *RowPtr1=&(M1.Values[Row1*M1.W16]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	unsigned W=M1.Width;
	unsigned W16=(M1.Width+3)&~0x3;
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
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
			vmul vf4,vf2,vf3
			vadd vf1,vf1,vf4
			sqc2 vf1,0(t1)
			addi t1,16				// next four numbers
			addi t2,16
			addi t0,-4				// counter-=4
			bgtz t0,iLoop
			nop
		nop		
		Escape:
	"					: 
						: "r" (W16), "r" (RowPtr1), "r" (RowPtr2),"r" (d), "r" (Stride1), "r" (Stride2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
}
#endif
#else      
void AddRowMultiple(Matrix &M1,unsigned Row1,float Multiplier,Matrix &M2,unsigned Row2)
{
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	unsigned W=(M1.Width+3)&~0x3;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov esi,[ebx]
		mov eax,Row1
		mul Stride1
		add edi,eax
		mov eax,Row2
		mul Stride2
		add esi,eax
		mov eax,W		//[edx+16]
		shr eax,2
		movss xmm0,Multiplier
		shufps xmm0,xmm0,0
		iLoop:
			movaps xmm2,[esi]  
			mulps xmm2,xmm0
			movaps xmm1,[edi]
			addps xmm1,xmm2
			movaps [edi],xmm1
			add edi,16
			add esi,16
		dec eax
		cmp eax,0
		jne iLoop
	}
}
#endif
          
void AddColumnMultiple(Matrix &M1,unsigned Col1,float Multiplier,Matrix &M2,unsigned Col2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M1.Height;i++)
		M1(i,Col1)+=Multiplier*M2(i,Col2);
#else
	float *Ptr1=&(M1.Values[Col1]);
	float *Ptr2=&(M2.Values[Col2]);
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	float *m=&Multiplier;
	asm __volatile__("
		.set noreorder
		add t1,zero,%2				// t1= address of matrix 1's row
		add t2,zero,%3
		lw t0,12(%1)
		blez t0,Escape				// for zero width matrix
		lwc1 $f3,0(%0)		// Multiplier
		nop
		iLoop:
			lwc1 $f1,0(t1)			// load v1[Y]
			lwc1 $f2,0(t2)			// load v1[Z]
			mul.s $f2,$f3,$f2
			add.s $f1,$f1,$f2
			swc1 $f1,0(t1)
			add t1,%6				// next  numbers
			add t2,%7
			addi t0,-1				// counter-=4
			bgtz t0,iLoop
			nop
		Escape:
	": 
	: "r" (m),"r"(&M2),"r" (Ptr1),"r" (Ptr2),"r" (Col1),"r" (Col2),"r" (Stride1),"r" (Stride2)
	: "$f1", "$f2", "$f3");
#endif
#else   
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	unsigned H16=(unsigned)(((unsigned)M1.Height+3)&~0x3);
	_asm
	{
		//mov ecx,dptr
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov esi,[ebx]
		mov eax,Col1
		imul eax,4
		add edi,eax
		mov eax,Col2
		imul eax,4
		add esi,eax
		mov eax,H16
		shr eax,2
		//inc eax
		movss xmm0,Multiplier
		shufps xmm0,xmm0,0         //    M2(5-eax,Col2)
//				movups [ecx],xmm0
		iLoop:
			movss xmm2,[esi]
			add esi,Stride2
			movss xmm3,[esi]
			add esi,Stride2
			movss xmm4,[esi]
			add esi,Stride2
			movss xmm5,[esi]
			shufps xmm2,xmm3,0x44
			shufps xmm2,xmm2,0x88
			shufps xmm4,xmm5,0x44
			shufps xmm4,xmm4,0x88
			shufps xmm2,xmm4,0x44
//				movups [ecx],xmm2
			mulps xmm2,xmm0
//				movups [ecx],xmm2
			push edi
			movss xmm1,[edi]
			add edi,Stride1
			movss xmm3,[edi]
			add edi,Stride1
			movss xmm4,[edi]
			add edi,Stride1
			movss xmm5,[edi]
			pop edi
			shufps xmm1,xmm3,0x44
			shufps xmm1,xmm1,0x88
			shufps xmm4,xmm5,0x44
			shufps xmm4,xmm4,0x88
			shufps xmm1,xmm4,0x44   
//				movups [ecx],xmm1
			addps xmm1,xmm2          // xmm1 contains the new 4 values
//				movups [ecx],xmm1
			movss [edi],xmm1
			add edi,Stride1
			shufps xmm1,xmm1,0xE5      // xmm1(0) contains xmm1(1)  // 11 10 01 01
//				movups [ecx],xmm1
			movss [edi],xmm1
			add edi,Stride1
			unpckhps xmm1,xmm1
//				movups [ecx],xmm1
			movss [edi],xmm1
			add edi,Stride1
			unpckhps xmm1,xmm1
//				movups [ecx],xmm1
			movss [edi],xmm1
			add edi,Stride1    
			add esi,Stride2

		dec eax
		cmp eax,0
		jne iLoop
	}
#endif
}

void CopyColumn(Matrix &M1,unsigned Col1,Matrix &M2,unsigned Col2)
{              
#ifdef CHECK_MATRIX_BOUNDS
	if(Col1>=M1.Width||Col2>=M2.Width||Col1<0||Col2<0||M2.Height>M1.Height)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M2.Height;i++)
		M1(i,Col1)=M2(i,Col2);
#else
	float *ColPtr1=&(M1.Values[Col1]);
	float *ColPtr2=&(M2.Values[Col2]);
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	asm __volatile__("
		.set noreorder
		lw t0,12(a2)				// t0= number of 4-wide columns
		add t1,zero,%0				// t1= address of matrix 1's row
		add t2,zero,%1
		blez t0,Escape				// for zero width matrix
		iLoop:
			addi t0,-1				// counter-=4
			lwc1 f1,0(t2)			// Load 4 numbers into vf2
			swc1 f1,0(t1)
			add t1,%2				// next four numbers
		bgtz t0,iLoop
			add t2,%3
		Escape:
	"					: 
						: "r" (ColPtr1), "r" (ColPtr2),"r" (Stride1), "r" (Stride2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif                             
#else
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx] 
		mov ecx,[ebx+12]
		cmp ecx,0
		jle Escape
		mov esi,[ebx]
		mov eax,Col1
		imul eax,4
		add edi,eax
		mov eax,Col2
		imul eax,4
		add esi,eax
		iLoop:
			mov eax,[esi]
			mov [edi],eax
			add edi,Stride1
			add esi,Stride2
		add ecx,-1
		cmp ecx,0
		jg iLoop
		Escape:
	}      
#endif
}

void CopyRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row1>=M1.Height||Row2>=M2.Height||Row1<0||Row2<0||M1.Width<M2.Width)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M2.Width;i++)
		M1(Row1,i)=M2(Row2,i);
#else
	float *RowPtr1=&(M1.Values[Row1*M1.W16]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	//unsigned W=M1.Width;
	//unsigned W16=(M1.Width+3)&~0x3;
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	asm __volatile__("
		.set noreorder
		lw t0,16(%5)					// t0= number of columns
		add t1,zero,%1					// t1= address of matrix 1's row
		add t2,zero,%2
		blez t0,Escape					// for zero width matrix
		iLoop:
			addi t0,-4				// counter-=4
			lqc2 vf1,0(t2)			// Load 4 numbers into vf2
			sqc2 vf1,0(t1)
			addi t1,16				// next four numbers
		bgtz t0,iLoop
			addi t2,16
		Escape:
		.set reorder
	"					:
						: "r" (&M1), "r" (RowPtr1), "r" (RowPtr2),"r" (Stride1), "r" (Stride2),"r" (&M2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	//unsigned W=(M1.Width+3)&~0x3;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov ecx,[ebx+16]
		cmp ecx,0
		jle Escape
		mov esi,[ebx]
		mov eax,Row1
		mul Stride1
		add edi,eax
		mov eax,Row2
		mul Stride2
		add esi,eax
		iLoop:
			movaps xmm2,[esi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
		add ecx,-4
		cmp ecx,0
		jg iLoop
		Escape:
	}
#endif
}             

// M2 is lower-triangular:

void CopyLTRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row1>=M1.Height||Row2>=M2.Height||Row1<0||Row2<0||M1.Width<M2.Width)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif      
#ifndef SIMD
	unsigned i;
	for(i=0;i<=Row2;i++)
		M1(Row1,i)=M2(Row2,i); 
	for(;i<M2.Height;i++)
		M1(Row1,i)=M2(i,Row2);   
#else
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov ecx,Row2
		mov esi,[ebx]
		mov eax,Row1
		mul Stride1
		add edi,eax
		mov eax,Row2
		mul Stride2
		add esi,eax
		cmp ecx,0
		jle Vert
		HLoop:
			mov eax,[esi]
			mov [edi],eax
			add edi,4
			add esi,4
			add ecx,-1
			cmp ecx,0  
		jg HLoop
		Vert:
		mov ecx,[ebx+16]
		mov eax,Row2
		sub ecx,eax   
		cmp ecx,0
		jle Escape
		VLoop:
			mov eax,[esi]
			mov [edi],eax
			add edi,4
			add esi,Stride2
			add ecx,-1
			cmp ecx,0  
		jg VLoop
		Escape:
	}
#endif
}

// Row2 > Row1:
void CopyLTRowColumn(Matrix &M,unsigned Row1,unsigned Row2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row1>=M.Height||Row2>=M.Height||Row1<0||Row2<0)
	{
		unsigned errorhere=1;
		errorhere++;
		errorhere;
		throw Matrix::BadSize();
	}
#endif      
#ifndef SIMD
	unsigned i;
	for(i=0;i<=Row1;i++)
		M(Row1,i)=M(Row2,i);
	for(i=Row1+1;i<M.Height;i++)
		M(i,Row1)=M(Row2,i);   
	M(Row1,Row1)=M(Row2,Row2);
#else                     
/*	unsigned i;
	for(i=0;i<=Row1;i++)
		M(Row1,i)=M(Row2,i);
	for(i=Row1+1;i<M.Height;i++)
		M(i,Row1)=M(Row2,i);   
	M(Row1,Row1)=M(Row2,Row2);*/
	unsigned Stride=M.W16*4;
	_asm
	{
		mov ebx,M
		mov edi,[ebx]
		mov ecx,Row1  
	//add ecx,-1
		mov esi,[ebx]
		mov eax,Row1
		mul Stride
		add edi,eax
		mov eax,Row2
		mul Stride
		add esi,eax
		cmp ecx,0
		jle Vert
		HLoop:
			mov eax,[esi]
			mov [edi],eax
			add edi,4
			add esi,4
			add ecx,-1
			cmp ecx,0
		jg HLoop    
		Vert:
        push edi     
		add edi,Stride
		add esi,4
		mov ecx,[ebx+16]
	add ecx,-1
		mov eax,Row1
		sub ecx,eax   
		cmp ecx,0
		jle Escape
		VLoop:
			mov eax,[esi]
        mov [edi],eax
			add esi,4
			add edi,Stride
			add ecx,-1
			cmp ecx,0  
		jg VLoop
        pop edi
		mov eax,[esi]
		mov [edi],eax
		Escape:
	}
#endif
}

void CopyRowToColumn(Matrix &M1,unsigned Col1,Matrix &M2,unsigned Row2)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<M1.Height;i++)
		M1(i,Col1)=M2(Row2,i);
#else        
	float *ColPtr1=&(M1.Values[Col1]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	unsigned Stride1=M1.W16*4;
	__asm
	{                
		mov ecx,M2
		mov eax,[ecx+16]
		mov ebx,ColPtr1
		mov ecx,RowPtr2
		cmp eax,0
		jl Escape
		iLoop:
			mov edx,[ecx]		// Load 1 number into f2
			mov [ebx],edx
			add ebx,Stride1		// next number
			add ecx,4
			add eax,-1			// counter-=1
		jnz iLoop
		Escape:
	}
#endif
#else
	float *ColPtr1=&(M1.Values[Col1]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	unsigned W=M2.Width;
	unsigned W16=(M2.Width+3)&~0x3;
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2              // t2= address of matrix 2's column
		blez t0,Escape				// for zero width matrix
		nop
		iLoop:
			lwc1 f1,0(t2)			// Load 1 number into f2
			swc1 f1,0(t1)
			addi t2,4                                    
			addi t1,%3				// next number
			addi t0,-1				// counter-=1
			bgtz t0,iLoop
			nop
		nop
		Escape:
	"					:
						: "r" (W16), "r" (ColPtr1), "r" (RowPtr2),"r" (Stride1), "r" (Stride2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}          

void CopyRowToColumn(Matrix &M1,unsigned Col1,unsigned StartRow,Matrix &M2,unsigned Row2)
{
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<M1.Height;i++)
		M1(StartRow+i,Col1)=M2(Row2,i);
#else        
	float *ColPtr1=&(M1.Values[StartRow*M1.W16+Col1]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	unsigned Stride1=M1.W16*4;
	__asm
	{                
		mov ecx,M2
		mov eax,[ecx+16]
		mov ebx,ColPtr1
		mov ecx,RowPtr2
		cmp eax,0
		jl Escape
		iLoop:
			mov edx,[ecx]		// Load 1 number into f2
			mov [ebx],edx
			add ebx,Stride1		// next number
			add ecx,4
			add eax,-1			// counter-=1
		jnz iLoop
		Escape:
	}
#endif
#else
	float *ColPtr1=&(M1.Values[StartRow*M2.W16+Col1]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	unsigned W=M2.Width;
	unsigned W16=(M2.Width+3)&~0x3;
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2              // t2= address of matrix 2's column
		blez t0,Escape				// for zero width matrix
		nop
		iLoop:
			lwc1 f1,0(t2)			// Load 1 number into f2
			swc1 f1,0(t1)
			addi t2,4                                    
			addi t1,%3				// next number
			addi t0,-1				// counter-=1
			bgtz t0,iLoop
			nop
		nop
		Escape:
	"					:
						: "r" (W16), "r" (ColPtr1), "r" (RowPtr2),"r" (Stride1), "r" (Stride2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}
#undef PLAYSTATION2            
   
void CopyColumnToRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Col2)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(M1.Height!=M2.Width)
		throw Matrix::BadSize();
#endif
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M1.Height;i++)
		M1(Row1,i)=M2(i,Col2);
#else
	float *RowPtr1=&(M1.Values[Row1*M1.W16]);
	float *ColPtr2=&(M2.Values[Col2]);
	unsigned W=M1.Width;
	unsigned W16=(M1.Width+3)&~0x3;
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of 4-wide columns
		add t1,zero,%1				// t1= address of matrix 1's row
		add t2,zero,%2              // t2= address of matrix 2's column
		blez t0,Escape				// for zero width matrix
		nop
		iLoop:
			lwc1 f1,0(t2)			// Load 1 number into f2
			swc1 f1,0(t1)
			addi t1,%4				// next four numbers
			addi t2,4
			addi t0,-1				// counter-=4
			bgtz t0,iLoop
			nop
		nop
		Escape:
	"					:
						: "r" (W16), "r" (RowPtr1), "r" (ColPtr2),"r" (Stride1), "r" (Stride2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}
 
void CopyRowToColumn(Matrix &M,unsigned R)
{        
#ifdef CHECK_MATRIX_BOUNDS
	if(M.Height!=M.Width)
		throw Matrix::BadSize();
#endif
#ifndef PLAYSTATION2
#ifndef SIMD
	for(unsigned i=0;i<M.Height;i++)
		M(i,R)=M(R,i);
#else        
	float *ColPtr1=&(M.Values[R]);
	float *RowPtr2=&(M.Values[R*M.W16]);
	unsigned Stride1=M.W16*4;
	__asm
	{                
		mov ecx,M
		mov eax,[ecx+16]
		mov ebx,ColPtr1
		mov ecx,RowPtr2
		cmp eax,0
		jl Escape
		iLoop:
			mov edx,[ecx]		// Load 1 number into f2
			mov [ebx],edx
			add ebx,Stride1		// next number
			add ecx,4
			add eax,-1			// counter-=1
		jnz iLoop
		Escape:
	}
#endif
#else
	float *ColPtr1=&(M.Values[R]);
	float *RowPtr2=&(M.Values[R*M.W16]);
	unsigned Stride=M.W16*4;
	asm __volatile__("
		.set noreorder
		add t0,zero,%0				// t0= number of rows or columns
		add t1,zero,%1				// t1= address of matrix's column
		add t2,zero,%2              // t2= address of matrix's row
		blez t0,Escape				// for zero width matrix
		nop
		iLoop:
			lw t4,0(t2)				// Load 1 number into t4
			sw t4,0(t1)
			addi t2,4                                    
			add t1,%3				// next number
			addi t0,-1				// counter-=1
			bgtz t0,iLoop
			nop
		nop
		Escape:
	"					:
						: "r" (M.Width), "r" (ColPtr1), "r" (RowPtr2),"r" (Stride)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
}

void RemoveRow(Matrix &M,unsigned Row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>=M.Height)
		throw Matrix::BadSize();
#endif
#ifndef SIMD
#if 1//ndef PLAYSTATION2
	for(unsigned i=Row;i<M.Height-1;i++)
		for(unsigned j=0;j<M.Width;j++)
			M(i,j)=M(i+1,j);
#else
	float *RowPtr=&(M.Values[Row*M.W16]);
	unsigned Stride=M.W16*4;
	asm __volatile__("
		.set noreorder
		add t1,zero,%2			// Height
		addi t1,-1				// Height-1
		sub t1,%1
		blez t1,iEscape
		nop
		add t0,zero,%0			// Row i
		iLoop:
			add t3,zero,t0		// Dest M(0,i)
			add t0,%4			// M(0,i+1)
			add t2,zero,t0 		// Source M(0,i+1)
			add t5,zero,%3
			jLoop:
				lqc2 vf1,0(t2)
				sqc2 vf1,0(t3)
				addi t3,16
				addi t2,16
				addi t5,-4
			bgtz t5,jLoop
			nop
			addi t1,-1
		bgtz t1,iLoop
		nop
		iEscape:
		.set noreorder
	"					:
						: "g"(RowPtr),"g"(Row),"g"(M.Height),"g"(M.Width),"g"(Stride)
						:);
#endif
#else
	float *RowPtr=&(M.Values[Row*M.W16]);
	unsigned Stride=M.W16*4;
	_asm
	{
		mov ebx,M
		mov ebx,[ebx+12]		// Height
		add ebx,-1				// Height-1
		sub ebx,Row
		cmp ebx,0
		je iEscape
		mov eax,RowPtr			// Row i
		iLoop:
			mov edi,eax        	// Dest M(i,0)
			add eax,Stride		// M(i+1,0)
			mov esi,eax         // Source M(i+1,0)
			mov ecx,M
			mov ecx,[ecx+16]
			jLoop:
				movaps xmm2,[esi]
				movaps [edi],xmm2
				add edi,16
				add esi,16
				add ecx,-4
				cmp ecx,0
			jg jLoop
			add ebx,-1
			cmp ebx,0
		jg iLoop
		iEscape:
	}
#endif         
	M.ResizeRetainingValues(M.Height-1,M.Width);
}                  

void RemoveColumn(Matrix &M,unsigned Col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Col>=M.Width)
		throw Matrix::BadSize();
#endif
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=Col;i<M.Width-1;i++)
		for(unsigned j=0;j<M.Height;j++)
			M(j,i)=M(j,i+1);
#else
	float *ColPtr=&(M.Values[Col]);
	unsigned Stride=M.W16*4;
	asm __volatile__("
		.set noreorder
		add t1,zero,%3		// Width
		addi t1,-1				// Height-1
		sub t1,%1
		blez t1,iEscape
		nop
		add t0,zero,%0			// Row i
		iLoop:
			add t3,zero,t0        	// Dest M(0,i)
			addi t0,4			// M(0,i+1)
			add t2,zero,t0         // Source M(0,i+1)
			add t5,zero,%2
			jLoop:
				lw t8,0(t2)
				sw t8,0(t3)
				add t3,%4
				add t2,%4
				addi t5,-1
			bgtz t5,jLoop
			nop
			addi t1,-1
		bgtz t1,iLoop
		nop
		iEscape:
		.set noreorder
	"					:
						: "g"(ColPtr),"g"(Col),"g"(M.Height),"g"(M.Width),"g"(Stride)
						:);
#endif                             
#else
	float *ColPtr=&(M.Values[Col]);
	unsigned Stride=M.W16*4;
	_asm
	{
		mov ebx,M
		mov ebx,[ebx+16]		// Width
		add ebx,-1				// Height-1
		sub ebx,Col
		cmp ebx,0
		je iEscape
		mov eax,ColPtr			// Row i
		iLoop:
			mov edi,eax        	// Dest M(0,i)
			add eax,4			// M(0,i+1)
			mov esi,eax         // Source M(0,i+1)
			mov ecx,M
			mov ecx,[ecx+12]
			jLoop:
				movss xmm2,[esi]
				movss [edi],xmm2
				add edi,Stride
				add esi,Stride
				add ecx,-1
				cmp ecx,0
			jg jLoop
			add ebx,-1
			cmp ebx,0
		jg iLoop
		iEscape:
	}
#endif
	M.ResizeRetainingValues(M.Height,M.Width-1);
}

void InsertColumn(Matrix &M,unsigned Col)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Col>M.Width)
		throw Matrix::BadSize();
#endif
	M.ResizeRetainingValues(M.Height,M.Width+1);
	for(unsigned i=M.Width-1;i>Col;i--)
		for(unsigned j=0;j<M.Height;j++)
			M(j,i)=M(j,i-1);
}                  

void InsertRow(Matrix &M,unsigned Row)
{
#ifdef CHECK_MATRIX_BOUNDS
	if(Row>M.Height)
		throw Matrix::BadSize();
#endif
	M.ResizeRetainingValues(M.Height+1,M.Width);
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=M.Height-1;i>Row;i--)
		for(unsigned j=0;j<M.Width;j++)
			M(i,j)=M(i-1,j);
#else
	float *RowPtr=&(M.Values[(M.Height-1)*M.W16]);      
	unsigned Stride=M.W16*4;
	asm __volatile__("
		.set noreorder
		add t2,zero,%2			// Height
		addi t2,-1				// Height-1
		sub t2,%1
		blez t2,iEscape
		nop
		add t1,zero,RowPtr		// Row i
		iLoop:
			add t4,zero,t1      // Dest M(i,0)
			sub t1,%3			// M(i-1,0)
			add t5,zero,t1		// Source M(i-1,0)
			add t3,zero,%4
			jLoop:
				lqc2 vf1,0(t5)
				sqc2 vf1,0(t4)
				addi t4,16
				addi t5,16
				addi t3,-4
			bgtz t3,jLoop
			nop
			addi t2,-1
		bgtz t2,iLoop
		nop
		iEscape:
	"					:
						: "r" (RowPtr),"r"(Row),"g"(M.Height),"g"(Stride),"g"(M.Width)
						:);
#endif
#else
	float *RowPtr=&(M.Values[(M.Height-1)*M.W16]);
	unsigned Stride=M.W16*4;
	_asm
	{
		mov ebx,M
		mov ebx,[ebx+12]		// Height
		add ebx,-1				// Height-1
		sub ebx,Row
		cmp ebx,0
		je iEscape
		mov eax,RowPtr			// Row i
		iLoop:
			mov edi,eax        	// Dest M(i,0)
			sub eax,Stride		// M(i-1,0)
			mov esi,eax         // Source M(i-1,0)
			mov ecx,M
			mov ecx,[ecx+16]
			jLoop:
				movaps xmm2,[esi]
				movaps [edi],xmm2
				add edi,16
				add esi,16
				add ecx,-4
				cmp ecx,0
			jg jLoop
			add ebx,-1
			cmp ebx,0
		jg iLoop
		iEscape:
	}
#endif
}

void CopyRow(Matrix &M1,unsigned Row1,unsigned Col1,Matrix &M2,unsigned Row2)
{
#ifndef SIMD
#ifndef PLAYSTATION2
	for(unsigned i=0;i<M1.Width;i++)
		M1(Row1,Col1+i)=M2(Row2,i);
#else
	float *RowPtr1=&(M1.Values[Row1*M1.W16+Col1]);
	float *RowPtr2=&(M2.Values[Row2*M2.W16]);
	asm __volatile__("
		.set noreorder
		lw t0,16(a3)					// t0= M2.Width
		add t1,zero,%0					// t1= address of matrix 1's row
		add t2,zero,%1
		blez t0,Escape					// for zero width matrix
		and t3,t1,0xF					// Are we aligned on a 16-byte boundary?
		beqz t3,iLoop
	// Non-aligned copy
		UnalignedLoop:
			lwc1 $f1,0(t2)			// Load 4 numbers into vf2
			swc1 $f1,0(t1)
			addi t1,4				// next number
			addi t0,-1				// counter-=4
		bgtz t0,UnalignedLoop
			addi t2,4
		b Escape
		iLoop:
			addi t0,-4				// counter-=4
			lqc2 vf1,0(t2)			// Load 4 numbers into vf2
			sqc2 vf1,0(t1)
			addi t1,16				// next four numbers
		bgtz t0,iLoop
			addi t2,16
		Escape:
		.set reorder
	"					:
						: "r" (RowPtr1), "r" (RowPtr2)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
#endif
#else
	unsigned Stride1=M1.W16*4;
	unsigned Stride2=M2.W16*4;
	unsigned W=M2.Width;
	_asm
	{
		mov edx,M1
		mov ebx,M2
		mov edi,[edx]
		mov esi,[ebx]
		mov eax,Row1
		mul Stride1
		add edi,eax   
		mov eax,Col1
		imul eax,4
		add edi,eax
		mov eax,Row2
		mul Stride2
		add esi,eax  
		mov eax,W
		mov ecx,Col1
		and ecx,3
		cmp ecx,0
		je iLoop
		UnalignedLoop:
			movss xmm2,[esi]
			movss [edi],xmm2
			add edi,4
			add esi,4
		dec eax    
		cmp eax,0
		jne UnalignedLoop
		jmp iEscape
		iLoop:
			movaps xmm2,[esi]
			movaps [edi],xmm2
			add edi,16
			add esi,16
		sub eax,4
		cmp eax,0
		jg iLoop
		iEscape:
	}
#endif  
}

#ifndef SIMD
#ifndef PLAYSTATION2
void ZeroRow(Matrix &M1,unsigned Row1)
{
	for(unsigned i=0;i<((M1.Width+3)&~3);i++)
		M1.Values[Row1*M1.W16+i]=0;
}
#else
void ZeroRow(Matrix &M1,unsigned Row1)
{
	float *RowPtr1=&(M1.Values[Row1*M1.W16]);
	unsigned Stride1=M1.W16*4;
	asm __volatile__("
		.set noreorder
		vsub vf1,vf0,vf0
		lw t0,16(%0)					// t0= number of 4-wide columns
		add t1,zero,%1					// t1= address of matrix 1's row
		blez t0,Escape					// for zero width matrix 
		nop
		iLoop:
			sqc2 vf1,0(t1)
			addi t0,-4				// counter-=4
		bgtz t0,iLoop
			addi t1,16				// next four numbers
		Escape:
	"					: 
						: "r" (&M1), "r" (RowPtr1),"r" (Stride1)
						: "$vf1", "$vf2", "$vf3", "$vf4", "$vf5", "$vf6");
}
#endif
#else
void ZeroRow(Matrix &M1,unsigned Row1)
{
	unsigned Stride1=M1.W16*4;
	unsigned W=M1.W16;
	_asm
	{
		mov edx,M1
		mov edi,[edx]
		mov eax,Row1
		mul Stride1
		add edi,eax
		xorps xmm0,xmm0
		mov eax,W
		shr eax,2
		iLoop:
			movaps [edi],xmm0
			add edi,16
		dec eax
		cmp eax,0
		jne iLoop
	}
}
#endif
#if 0
void MultiplyByTranspose(Matrix &Res,Matrix &B,ListOfPointers<unsigned*>&Row,ListOfPointers<unsigned*>&Col)
{
#ifndef PLAYSTATION2
	unsigned j;
	for(unsigned i=0;i<Row.NumValues;i++)
	{
		unsigned R=*Row[i];
		for(j=0;j<Col.NumValues;j++)
		{            
			unsigned C=*Col[j];
			Res.Values[i*Res.W16+j]=MultiplyRows(B,R,C);
		}
	}
#else
	Row; // a2 is referenced below.
	Col; // a3 is referenced below.
	Matrix *Resaddr=&Res;
	Matrix *BAddr=&B;
	float *ResMin=Res.Values;
	float *ResMax=&(Res.Values[Res.BufferSize-1]);
//	ListOfPointers<unsigned*> *RowAddr=&Row;
	asm __volatile__("
		.set noreorder
		lw t0,16(%1)
		blez t0,Escape   
		lw s2,0(a2)						// Row.Values
		lw s4,0(a3)						// Col.Values
		lw t9,0(%0)						// Res.Values[i.i]
		lw t7,8(%0)						// Res.W16
		sll s0,t7,2						// Res.W16*4=ResStride
		add t7,s0,4						// i,0
		
		iLoop:
			add t6,zero,s4			// Col.Values[0]
			add s6,zero,t9
			
			lw t5,0(%1)				// Values[0]
			lw t4,8(%1)				// M.W16
			sll t4,t4,2				// M.W16*4=Stride
			lw t8,0(s2)				// t3=R1
			lw t3,0(t8)				// t3=R1
			mul t3,t3,t4			// t3=R1*4*M.W16
			add t5,t5,t3			// &M(R1,0)
			
			jLoop:
				lw t0,16(%1)
				add t1,zero,t5
				lw t2,0(%1)				// Values[0]
				lw t8,0(t6)				// t3=R2
				lw t3,0(t8)				// t3=R2
				mul t3,t3,t4			// t3=R2*4*M.W16
				add t2,t2,t3			// &M(R1,0)
				vmulax ACC,vf0,vf0x
				addi t0,-4			// counter-=1
				blez t0,EndLoop
				Loop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmadda ACC,vf1,vf2
					addi t1,16			// next four numbers
					addi t0,-4			// counter-=1
				bgtz t0,Loop
					addi t2,16
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
				EndLoop:
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmadd vf3,vf1,vf2
					
					vaddx vf4,vf3,vf3x
					vaddz vf4,vf4,vf3z
					vaddy vf4,vf4,vf3y	// total is in vf3w
					vmul vf4,vf4,vf0
					
					and t1,s6,0xF
					and t2,s6,0xFFFFFFF0
				beq t1,12,esccase		// W
				nop
				beq t1,8,esccase		// Z
				vmr32 vf4,vf4
				beq t1,4,esccase		// Y
				vmr32 vf4,vf4
				b emptycase				// just store, don't add to what's there.
				vmr32 vf4,vf4		// X
			esccase:
				lqc2 vf3,0(t2)
				vadd vf4,vf4,vf3
			emptycase:
				sqc2 vf4,0(t2)
//				blt t2,%3,Dodgy
//				bgt t2,%4,Dodgy
				addi t6,4
				lw t8,0(t6)				// t3=R1
			bnez t8,jLoop
				add s6,s6,4
			addi s2,4
			lw t8,0(s2)				// t3=R1
		bnez t8,iLoop
			add t9,t9,t7
//		b Escape
//		nop
//		Dodgy:
//			addi s2,4
//			addi s2,4
//			addi s2,4
//			addi s2,4
		Escape:
		.set reorder   
	"					:  
						:  "r" (Resaddr),"r" (BAddr), "r" (BAddr),"r"(ResMin),"r"(ResMax)
						:  "s0","s2","s6");
#endif
}

void MultiplyByTranspose(Matrix &Res,Matrix &B,ListOfPointers<unsigned*>&Row)
{
#ifndef PLAYSTATION2
	unsigned i,j;
	unsigned **R;
	unsigned **C;
	for(i=0,R=Row.Values;*R;i++,R++)
	{
		for(j=i,C=R;*C;j++,C++)
		{            
			Res.Values[j*Res.W16+i]=MultiplyRows(B,**R,**C);
		}
	}
#else
//	Matrix *ResAddr=&Res;
//	float *ResMin=Res.Values;
	//float *ResMax=&(Res.Values[Res.BufferSize-1]);
	asm __volatile__("
		.set noreorder
		lw t0,16(%1)
		blez t0,Escape   
		lw s2,0(a2)						// Row.Values
		lw t9,0(%0)						// Res.Values[i.i]
		lw t7,8(%0)						// Res.W16
		muli s0,t7,4						// Res.W16*4=ResStride
		add t7,s0,4						// i,i
		
		iLoop:
			add t6,zero,s2
			add s6,zero,t9
			
			lw t5,0(%1)				// Values[0]
			lw t4,8(%1)				// M.W16
			muli t4,t4,4			// M.W16*4=Stride
			lw t8,0(s2)				// t3=R1
			lw t3,0(t8)				// t3=R1
			mul t3,t3,t4			// t3=R1*4*M.W16
			add t5,t5,t3			// &M(R1,0)
			
			jLoop:
				lw t0,16(%1)
				add t1,zero,t5
				lw t2,0(%1)				// Values[0]
				lw t8,0(t6)				// t3=R2
				lw t3,0(t8)				// t3=R2
				mul t3,t3,t4			// t3=R2*4*M.W16
				add t2,t2,t3			// &M(R1,0)
				vmulax ACC,vf0,vf0x
				ble t0,4,EndLoop
				//nop
				Loop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmadda ACC,vf1,vf2
					addi t1,16			// next four numbers
					addi t0,-4			// counter-=1
				bgt t0,4,Loop
					addi t2,16
				EndLoop:
					lqc2 vf1,0(t1)		// Load 4 numbers into vf1
					lqc2 vf2,0(t2)		// Load 4 numbers into vf2
					vmadd vf3,vf1,vf2
					
					vaddx vf4,vf3,vf3x
					vaddz vf4,vf4,vf3z
					vaddy vf4,vf4,vf3y	// total is in vf3w
					vmul vf4,vf4,vf0
					
					and t2,s6,0xFFFFFFF0
					and t1,s6,0xF
				beq t1,12,esccase		// W
				nop
				beq t1,8,esccase		// Z
				vmr32 vf4,vf4
				beq t1,4,esccase		// Y
				vmr32 vf4,vf4
				b emptycase				// just store, don't add to what's there.
				vmr32 vf4,vf4			// X
			esccase:
				lqc2 vf3,0(t2)
				vadd vf4,vf4,vf3
			emptycase:
				sqc2 vf4,0(t2)
				//blt t2,%3,Dodgy
				//nop
				//bgt t2,%4,Dodgy
				addi t6,4
				lw t8,0(t6)				// t3=R1
				add s6,s6,s0
			bnez t8,jLoop
			nop
			addi s2,4
			lw t8,0(s2)				// t3=R1
			add t9,t9,t7
		bnez t8,iLoop
		nop
		//b Escape
		//nop
		//Dodgy:
		//	addi t6,4
		//	addi t6,4
		//	addi t6,4
		//	addi t6,4
		Escape:
		.set reorder   
	"					:  
						:  "r" (&Res),"r" (&B), "r" (&Row),"r" (Res.Values)//,"r"(ResMax)
						:  "s0","s2","s6");				
					
/*	unsigned j;
	for(unsigned i=0;i<Row.NumValues;i++)
	{
		unsigned R=*Row[i];
		for(j=i;j<Row.NumValues;j++)
		{            
			unsigned C=*Row[j];
			Res.Values[j*Res.W16+i]=MultiplyRows(B,R,C);
		}
	}*/
#endif
}


void MultiplyByTranspose(Matrix &Res,Matrix &A,Matrix &B,ListOfPointers<unsigned*>&Row)
{
	unsigned i,j;
	unsigned **R;
	unsigned **C;
	for(i=0,R=Row.Values;*R;i++,R++)
	{
		for(j=i,C=R;*C;j++,C++)
		{            
			Res.Values[j*Res.W16+i]=MultiplyRows(A,**R,B,**C);
		}
	}       
}
void MultiplyByTranspose(Matrix &Res,Matrix &A,ListOfPointers<unsigned*>&Row,Matrix &B,ListOfPointers<unsigned*>&Col)
{
#ifndef PLAYSTATION2    
#ifndef SIMD
	unsigned j;
	for(unsigned i=0;i<Row.NumValues;i++)
	{
		unsigned R=*Row[i];
		for(j=0;j<Col.NumValues;j++)
		{
			unsigned C=*Col[j];
			Res.Values[i*Res.W16+j]=MultiplyRows(A,R,B,C);
		}
	}
#else
	if(!A.Width)
		return;
	float Result;
	float *r=&Result;
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
		mov esi,A
		mov eax,[esi+16]
		and eax,3
		cmp eax,0
		je Mask4  
		cmp eax,3
		je Mask3
		cmp eax,2
		je Mask2
		//Mask1:
			mov ecx,tt1
			jmp EndMask
		Mask2:
			mov ecx,tt2
			jmp EndMask
		Mask3:
			mov ecx,tt3
			jmp EndMask
		Mask4:
			mov ecx,tt4
		EndMask:     
		mov tt,ecx
	}                   
	ListOfPointers<unsigned*> *CC=&Col;
	for(unsigned i=0;i<Row.NumValues;i++)
	{
		unsigned R=*Row[i];           
		float *RowPtr=&(Res.Values[i*Res.W16]);
	_asm
	{
		mov esi,A
		mov eax,[esi+8]
		imul eax,4		// AStride
		mov ecx,[esi]   // A.Values
		mul R
		add ecx,eax     // A(Row,0)

		mov esi,RowPtr        
		mov ebx,CC		// &Col
		mov ebx,[ebx]	// Col.Values
		jLoop:    
		push ecx
		push ebx
		push esi

		mov esi,B
		mov edi,[esi]	// B.Values
		mov eax,[esi+8] // B.W16
		imul eax,4		// BStride

		mov ebx,[ebx]
		mov ebx,[ebx]   // *Col[j]==column index
		mul ebx         // eax= column index * BStride
		mov ebx,edi     // B.Values
		add ebx,eax		// B(Col,0)

		mov esi,A

		xorps xmm0,xmm0
		mov eax,[esi+16]

		cmp eax,16
		jl End_Macro_Loop

		Macro_Loop:
			movaps xmm1,[ecx]
			movaps xmm2,[ecx+16]
			movaps xmm3,[ecx+32]
			movaps xmm4,[ecx+48]
			mulps xmm1,[ebx]
			mulps xmm2,[ebx+16]
			mulps xmm3,[ebx+32]
			mulps xmm4,[ebx+48]
			addps xmm0,xmm1
			addps xmm0,xmm2
			addps xmm0,xmm3
			addps xmm0,xmm4
			add ecx,64
			add ebx,64
			add eax,-16
			cmp eax,15
		jg Macro_Loop
	End_Macro_Loop:
		cmp eax,4
		jle Tail
		Dot_Product_Loop:
			movaps xmm1,[ecx]
			mulps xmm1,[ebx]
			addps xmm0,xmm1
			add ecx,16
			add ebx,16
			add eax,-4
			cmp eax,4
		jg Dot_Product_Loop
		Tail:
		movaps xmm1,[ecx]
		mulps xmm1,[ebx]
		mov eax,tt
		movups xmm2,[eax]
		andps xmm1,xmm2
		addps xmm0,xmm1
		//AddUp:
		// Now add the four sums together:
		movhlps xmm1,xmm0
		addps xmm0,xmm1         // = X+Z,Y+W,--,--
		movaps xmm1,xmm0
		shufps xmm1,xmm1,1      // Y+W,--,--,--
		addss xmm0,xmm1
		mov esi,r;
		pop esi               	// Res.Values[i*Res.W16+j]
		movss [esi],xmm0
		add esi,4
		pop ebx   
		pop ecx
		add ebx,4
		mov eax,[ebx]
		cmp eax,0
		jne jLoop
	}

			//Res.Values[i*Res.W16+j]=Result;

	   //	}
	}
#endif
#else 
	unsigned j;
	for(unsigned i=0;i<Row.NumValues;i++)
	{
		unsigned R=*Row[i];
		for(j=0;j<Col.NumValues;j++)
		{
			unsigned C=*Col[j];
			Res.Values[i*Res.W16+j]=MultiplyRows(A,R,B,C);
		}
	}
#endif
}
#endif
}
}
