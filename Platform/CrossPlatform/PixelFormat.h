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
			,R_8_UNORM
			,R_8_SNORM
			,R_32_UINT
			,RG_32_UINT
			,RGB_32_UINT
			,RGBA_32_UINT
			// depth formats:
			,D_32_FLOAT// DXGI_FORMAT_D32_FLOAT or GL_DEPTH_COMPONENT32F
		};
	}
}
