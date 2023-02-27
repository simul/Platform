#pragma once

namespace platform
{
	namespace math
	{
		union FP32
		{
			unsigned int u;
			float f;
			struct
			{
				unsigned int Mantissa : 23;
				unsigned int Exponent : 8;
				unsigned int Sign : 1;
			};
		};

		union FP16
		{
			unsigned short u;
			struct
			{
				unsigned int Mantissa : 10;
				unsigned int Exponent : 5;
				unsigned int Sign : 1;
			};
		};

		unsigned short ToFloat16(float f);
		float ToFloat32(unsigned short f);
	}
}