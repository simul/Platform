#include "Float16.h"

using namespace platform;
using namespace math;
	

bool platform::math::table=false;
unsigned short platform::math::eLut[1 << 9];

void platform::math::initELut()
{
    for (int i = 0; i < 0x100; i++)
    {
		int e = (i & 0x0ff) - (127 - 15);
		if (e <= 0 || e >= 30)
		{
			eLut[i]         = 0;
			eLut[i | 0x100] = 0;
		}
		else
		{
			eLut[i]         = (unsigned short)(e << 10);
			eLut[i | 0x100] = (unsigned short)((e << 10) | 0x8000);
		}
    }
}
