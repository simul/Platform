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
			,RGBA_8_UNORM
			,RGBA_8_SNORM
			,R_32_UINT
		};
	}
}
