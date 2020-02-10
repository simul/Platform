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
// MMXRand.h
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
#define _MMXRand_
namespace simul
{
	namespace math
	{
		class MMXTwister
		{
			protected:
				enum
					{
					N = 624,
					M = 397,
					MAGIC = 0x9908b0df
					};
				unsigned int m_state[N];
				unsigned int m_hashed[N]; // compute all at once using mmx
				int m_next;

			protected:
				static unsigned int Hash(unsigned int x)
					{
					x ^= (x >> 11);
					x ^= (x <<  7) & 0x9d2c5680U;
					x ^= (x << 15) & 0xefc60000U;
					return ( x ^ (x >> 18) );
					}
				unsigned int hiBit( unsigned int u )  const { return u & 0x80000000U; }
				unsigned int loBit( unsigned int u )  const { return u & 0x00000001U; }
				unsigned int loBits( unsigned int u ) const { return u & 0x7fffffffU; }
				unsigned int mixBits( unsigned int u, unsigned int v ) const
					{ 
					return hiBit(u) | loBits(v); 
					}
				unsigned int twist( unsigned int m, unsigned int s0, unsigned int s1 ) const
					{ 
					return m ^ (mixBits(s0,s1)>>1) ^ (loBit(s1) ? MAGIC : 0U); 
					}

				void DoTheTwist();
				void HashIt();

				void DoTheTwistC();
				void DoTheTwistMMX();
				void HashItC();
				void HashItASM();
				void HashItMMX();

				virtual void Reload()
					{
					DoTheTwist();
					HashIt();
					m_next = N;
					}

			public:
				MMXTwister()
					{
					Seed();	// calls Reload()
					}
				virtual ~MMXTwister()
				{
				}
				void Seed(unsigned int s=0);

				unsigned int randInt()
					{
					if (--m_next < 0)
						Reload();
					return m_hashed[m_next];
					}

				double rand()
					{ return double(randInt()) * 2.3283064370807974e-10; }

				double rand( const double n )
					{ return rand() * n; }

				double randExc()
					{ return double(randInt()) * 2.3283064365386963e-10; }

				double randExc( const double n )
					{ return randExc() * n; }

				unsigned int randInt( unsigned int n )
					{ return int( randExc() * (n+1) ); }
					// Is there a faster/better way?

				unsigned int operator()()
					{
					return randInt();
					}

				unsigned int operator()(unsigned int n)
					{
					return randInt(n);
					}
			};
	}
}
#endif

