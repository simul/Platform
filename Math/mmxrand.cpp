// Mersenne Twister random number generator
// Based on code by Makoto Matsumoto, Takuji Nishimura, Shawn Cokus, and Richard J. Wagner

// The Mersenne Twister is an algorithm for generating random numbers.  It
// was designed with consideration of the flaws in various other generators.
// The period, 2^19937-1, and the order of equidistribution, 623 dimensions,
// are far greater.  The generator is also fast; it avoids multiplication and
// division, and it benefits from caches and pipelines.  For more information
// see the inventors' web page at 
//
//		http://www.math.keio.ac.jp/~matumoto/emt.html

// Reference
// M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-Dimensionally
// Equidistributed Uniform Pseudo-Random Number Generator", ACM Transactions on
// Modeling and Computer Simulation, Vol. 8, No. 1, January 1998, pp 3-30.

// Copyright (C) 2000  Matthew Bellew
// 
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// The original code included the following notice:
//
//     Copyright (C) 1997, 1999 Makoto Matsumoto and Takuji Nishimura.
//     When you use this, send an email to: matumoto@math.keio.ac.jp
//     with an appropriate reference to your work.
//
// It would be nice to CC: rjwagner@writeme.com, cokus@math.washington.edu, and matthewb@westside.com
// when you write.




//-------------------------------------
// MMXRand.cpp
//
//	written by Matthew Bellew (July 2000)
//		http://www.bellew.net/rand/MMXRand.html

//
// Derived from Richard Wagner's implementation
//		http://www-personal.engin.umich.edu/~wagnerr/MersenneTwister.html
// of MersenneTwister
//		http://www.math.keio.ac.jp/~matumoto/emt.html
//


#ifndef _MMXRand_
#include "mmxrand.h"
#endif

#include "stdlib.h"
#include "time.h"

#ifdef _MSC_VER
#pragma optimize("",off)
#endif
using namespace simul;
using namespace math;

void MMXTwister::Seed(unsigned int seed)
	{
 	if (seed == 0)
		{
		seed = 4357U;
		}

		unsigned int *s;
		int i;
		for( i = N, s = m_state; i-- ; s++)
		{
			*s	= seed & 0xffff0000;
			*s	|= ( (seed *= 69069U)++ & 0xffff0000 ) >> 16;
			(seed *= 69069U)++;
		}
		Reload();
	}


//
// Implementation of MersenneTwister using the MMX instruction
// by Matthew Bellew (July 2000)
//
void MMXTwister::DoTheTwist()
	{
#ifdef SIMUL_USE_MMX_TWIST
	DoTheTwistMMX();
#else
	DoTheTwistC();
#endif
	}


void MMXTwister::DoTheTwistC()
{
	unsigned int *p = m_state;
	int i;
	for (i = N - M; i--; )
	{
		*p = twist(p[M], p[0], p[1]);
		*p++;
	}
	unsigned int *q = m_state;
	for (i = M; --i; )
	{
		*p = twist(*q++, p[0], p[1]);
		*p++;
	}
	*p = twist( *q, p[0], m_state[0] );
}


void MMXTwister::DoTheTwistMMX()
	{
#ifdef SIMD
	static __int64 doubleZero = 0x0000000000000000;
	static __int64 doubleOne  = 0x0000000100000001;
	static __int64 doubleMagic= 0x9908b0df9908b0df;
	static __int64 doubleLow  = 0x7fffffff7fffffff;
_asm
	{
	lea edi, DWORD PTR [ecx+4]	// skip vtable
	lea	esi, DWORD PTR [ecx+4]	// p = state

; for ( i = N - M; i--; )
;   *p++ = twist(p[M], p[0], p[1]);

	// NOTE
	// You can actually do three DWORDs at a time
	// by using the regular integer registers
	// However, it only helps performance about 10%

	mov	ecx, 113		; (624-397)/2=113 (+1)
$LOOP1:
	movq	mm1, QWORD PTR [esi]
	movq	mm2, QWORD PTR [esi+4]
	movq	mm0, mm1
	pxor	mm0, mm2
	pand	mm2, [doubleOne]
	pand	mm0, [doubleLow]
	pxor	mm0, mm1
	pcmpeqd	mm2, [doubleOne]
	psrld	mm0, 1
	pand	mm2, [doubleMagic]
	pxor	mm0, mm2
	pxor	mm0, QWORD PTR [esi+(397*4)]
	add		esi, 8
	dec		ecx
	movq	QWORD PTR [esi-8], mm0
	jne SHORT $LOOP1

	// left over
	mov	ebx, DWORD PTR [esi]
	mov	edx, DWORD PTR [esi+4]
	mov	eax, ebx
	xor	eax, edx
	and	dl, 1
	and	eax, 7ffffffeH
	xor	eax, ebx
	shr	eax, 1
	neg	dl
	sbb	edx, edx
	and	edx, 9908b0dfH
	xor	eax, edx
	xor	eax, DWORD PTR [esi+(397*4)]
	mov	DWORD PTR [esi], eax
	add	esi, 4

; for( i = M; --i; )
;   *p++ = twist( p[M-N], p[0], p[1] );

	mov	ecx, 198		; 397/2 = 198 (+1)
$LOOP2:
	movq	mm1, QWORD PTR [esi]
	movq	mm2, QWORD PTR [esi+4]
	movq	mm0, mm1
	pxor	mm0, mm2
	pand	mm2, [doubleOne]
	pand	mm0, [doubleLow]
	pxor	mm0, mm1
	pcmpeqd	mm2, [doubleOne]
	psrld	mm0, 1
	pand	mm2, [doubleMagic]
	pxor	mm0, mm2
	pxor	mm0, QWORD PTR [esi+(397-624)*4]
	add		esi, 8
	dec		ecx
	movq	QWORD PTR [esi-8], mm0
	jne SHORT $LOOP2

; *p = twist( p[M-N], p[0], state[0] );
	mov	ebx, DWORD PTR [esi]
	mov	edx, DWORD PTR [edi]
	mov	eax, ebx
	xor	eax, edx
	and	dl, 1
	and	eax, 7ffffffeH
	xor	eax, ebx
	shr	eax, 1
	neg	dl
	sbb	edx, edx
	and	edx, 9908b0dfH
	xor	eax, edx
	xor	eax, DWORD PTR [esi+(397-624)*4]
	mov	DWORD PTR [esi], eax

	emms
	}
#endif
	}

	
void MMXTwister::HashIt()
	{
#ifdef SIMUL_USE_MMX_TWIST
	HashItMMX();
#else
	HashItC();
#endif
	}


void MMXTwister::HashItC()
	{
    for (int i=0 ; i<N ; i++)
		m_hashed[i] = Hash(m_state[i]);
	}


void MMXTwister::HashItASM()
	{
#ifdef SIMD
	_asm
	{
	lea esi, DWORD PTR [ecx+4]
	lea edi, DWORD PTR [ecx+624*4+4]
	mov	ecx, 623
$LOOP:
;		x=m_state[i]
	mov	eax, DWORD PTR [esi+ecx*4]
;		x ^= (x >> 11);
	mov	edx, eax
	shr	edx, 11
	xor	eax, edx
;		x ^= (x <<  7) & 0x9d2c5680U;
	mov	edx, eax
	shl	edx, 7
	and	edx, 9d2c5680H
	xor	eax, edx
;		x ^= (x << 15) & 0xefc60000U;
	mov	edx, eax
	shl	edx, 15
	and	edx, 0efc60000H
	xor	eax, edx
;		x = ( x ^ (x >> 18) );
	mov	edx, eax
	shr	edx, 18
	xor	eax, edx
;		m_hashed[i] = x
	mov	DWORD PTR [edi+ecx*4], eax
	sub	ecx, 1
	jns	SHORT $LOOP
	}
#endif
}


void MMXTwister::HashItMMX()
	{
#ifdef SIMD
	static __int64 mask1 = 0x9d2c56809d2c5680;
	static __int64 mask2 = 0xefc60000efc60000;

	_asm
	{
	lea		esi, DWORD PTR [ecx+4]
	lea		edi, DWORD PTR [ecx+624*4+4]
	mov		ecx, 622
$LOOP:
;		x=m_state[i]
	movq	mm0, QWORD PTR [esi+ecx*4]
;		x ^= (x >> 11);
	movq	mm1, mm0
	psrld	mm1, 11		; shift right logical???
	pxor	mm0, mm1
;		x ^= (x <<  7) & 0x9d2c5680U;
	movq	mm1, mm0
	pslld	mm1, 7
	pand	mm1, QWORD PTR [mask1]
	pxor		mm0, mm1
;		x ^= (x << 15) & 0xefc60000U;
	movq	mm1, mm0
	pslld	mm1, 15
	pand	mm1, QWORD PTR [mask2]
	pxor		mm0, mm1
;		x = ( x ^ (x >> 18) );
	movq	mm1, mm0
	psrld	mm1, 18
	pxor	mm0, mm1
;		m_hashed[i] = x
	movq	QWORD PTR [edi+ecx*4], mm0
	sub		ecx, 2
	jns	SHORT $LOOP
	
	emms
	}
#endif
}


//---------------------------------
// main.cpp
//


#include "stdlib.h"
#include "stdio.h"


class CTwister : public MMXTwister
{
protected:
	virtual void Reload()
		{
		DoTheTwistC();
		HashItC();
		m_next = N;
		}
};