#pragma once
namespace simul
{
	namespace crossplatform
	{
		//! A cross-platform equivalent to the OpenGL and DirectX pixel formats
		enum PixelFormat
		{
			UNKNOWN
			,R_16_FLOAT
			,RGBA_16_FLOAT
			,RGBA_32_FLOAT
			,RGB_32_FLOAT
			,RG_32_FLOAT
			,R_32_FLOAT
			,LUM_32_FLOAT
			,INT_32_FLOAT
			,RGBA_8_UNORM
			,RGBA_8_SNORM
			,RGB_8_UNORM
			,RGB_8_SNORM
			,R_8_UNORM
			,R_8_SNORM
			,R_32_UINT
			,RG_32_UINT
			,RGB_32_UINT
			,RGBA_32_UINT
			// depth formats:
			,D_32_FLOAT// DXGI_FORMAT_D32_FLOAT or GL_DEPTH_COMPONENT32F
			,D_16_UNORM
		};
		inline int GetElementSize(PixelFormat p)
		{
			switch(p)
			{
			case RGBA_32_FLOAT:
			case RGB_32_FLOAT:
			case RG_32_FLOAT:
			case R_32_FLOAT:
			case LUM_32_FLOAT:
			case INT_32_FLOAT:
			case D_32_FLOAT:
				return sizeof(float);
			case R_32_UINT:
			case RG_32_UINT:
			case RGB_32_UINT:
			case RGBA_32_UINT:
				return sizeof(unsigned int);
			case R_16_FLOAT:
			case RGBA_16_FLOAT:
			case D_16_UNORM:
				return sizeof(short);
			case RGBA_8_UNORM:
			case RGBA_8_SNORM:
			case R_8_UNORM:
			case R_8_SNORM:
				return sizeof(char);
			default:
				return 0;
			};
		}
		inline int GetElementCount(PixelFormat p)
		{
			switch(p)
			{
			case RGBA_32_FLOAT:
			case RGBA_32_UINT:
			case RGBA_16_FLOAT:
			case RGBA_8_UNORM:
			case RGBA_8_SNORM:
				return 4;
			case RGB_32_FLOAT:
			case RGB_32_UINT:
				return 3;
			case RG_32_FLOAT:
			case RG_32_UINT:
				return 2;
			case R_32_FLOAT:
			case LUM_32_FLOAT:
			case INT_32_FLOAT:
			case D_32_FLOAT:
			case R_32_UINT:
			case R_16_FLOAT:
			case D_16_UNORM:
			case R_8_UNORM:
			case R_8_SNORM:
				return 1;
			default:
				return 0;
			};
		}
		inline int GetByteSize(PixelFormat p)
		{
			return GetElementSize(p)*GetElementCount(p);
		}
	}
}
