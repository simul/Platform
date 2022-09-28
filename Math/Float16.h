#pragma once

namespace platform
{
	namespace math
	{
		union uif
		{
			unsigned int	i;
			float			f;
		};
		extern bool table;
		extern unsigned short eLut[1 << 9];
		void initELut();
		inline short ToFloat16(float f)
		{
			short _h=0;
			if (f == 0)
			{
				_h = 0;
				return _h;
			}
			else
			{
				if(!table)
					initELut();
				uif x;
				x.f = f;
				int e = (x.i >> 23) & 0x000001ff;
				e = eLut[e];
				if (e)
				{
					_h = (short)(e + (((x.i & 0x007fffff) + 0x00001000) >> 13));
					return _h;
				}
				else
				{
					unsigned int i = (x.i);
					int s =  (i >> 16) & 0x00008000;
					int e = ((i >> 23) & 0x000000ff) - (127 - 15);
					int m =   i        & 0x007fffff;
					if (e <= 0)
					{
						if (e < -10)
						{
							_h=(short)s;
							return _h;
						}
						m = m | 0x00800000;
						int t = 14 - e;
						int a = (1 << (t - 1)) - 1;
						int b = (m >> t) & 1;
						m = (m + a + b) >> t;

						_h=(short)(s | m);
						return _h;
					}
					else if (e == 0xff - (127 - 15))
					{
						if (m == 0)
						{
							_h=(short)(s | 0x7c00);
							return _h;
						}
						else
						{
							m >>= 13;
							_h=(short)(s | 0x7c00 | m | (m == 0));
							return _h;
						}
					}
					else
					{
						m = m + 0x00000fff + ((m >> 13) & 1);
						if (m & 0x00800000)
						{
							m =  0;		// overflow in significand,
							e += 1;		// adjust exponent
						}
						if (e > 30)
						{
							// Cause a hardware floating point overflow;
							_h= (short)(s | 0x7c00);
							return _h;
						}
						_h= (short)(s | (e << 10) | (m >> 13));
						return _h;
					}
				}
			}
		}

	}
}