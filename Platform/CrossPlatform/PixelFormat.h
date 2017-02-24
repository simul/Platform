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
			,RGB_11_11_10_FLOAT
			,RG_32_FLOAT
			,RG_16_FLOAT
			,R_32_FLOAT
			,LUM_32_FLOAT
			,INT_32_FLOAT
			,RGBA_8_UNORM
			,RGBA_8_UNORM_COMPRESSED
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
			,D_24_UNORM_S_8_UINT
			,D_16_UNORM
		};
		//! This refers to the type of a shader resource, which should be compatible with the type of any resource assigned to it.
		enum class ShaderResourceType
		{
			UNKNOWN=0
			,RW		=1
			,ARRAY	=2
			,MS		=4
			,TEXTURE_1D					=8 
			,TEXTURE_2D					=16 
			,TEXTURE_3D					=32
			,TEXTURE_CUBE				=64
			,SAMPLER					=128
			,BUFFER						=256
			,CBUFFER					=512
			,TBUFFER					=1024
			,BYTE_ADDRESS_BUFFER		=2048
			,STRUCTURED_BUFFER			=4096
			,APPEND_STRUCTURED_BUFFER	=8192
			,CONSUME_STRUCTURED_BUFFER	=16384
			,TEXTURE_2DMS				=TEXTURE_2D|MS  
			,RW_TEXTURE_1D				=TEXTURE_1D|RW  
			,RW_TEXTURE_2D				=TEXTURE_2D|RW  
			,RW_TEXTURE_3D				=TEXTURE_3D|RW  
			,RW_BUFFER					=BUFFER|RW
			,RW_BYTE_ADDRESS_BUFFER		=RW|BYTE_ADDRESS_BUFFER
			,RW_STRUCTURED_BUFFER		=RW|STRUCTURED_BUFFER
			,TEXTURE_1D_ARRAY			=TEXTURE_1D|ARRAY
			,TEXTURE_2D_ARRAY 			=TEXTURE_2D|ARRAY  
			,TEXTURE_3D_ARRAY			=TEXTURE_3D|ARRAY   
			,TEXTURE_2DMS_ARRAY			=TEXTURE_2D|MS|ARRAY  
			,TEXTURE_CUBE_ARRAY 		=TEXTURE_CUBE|ARRAY   
			,RW_TEXTURE_1D_ARRAY		=RW|TEXTURE_1D|ARRAY
			,RW_TEXTURE_2D_ARRAY		=RW|TEXTURE_2D|ARRAY  
			,RW_TEXTURE_3D_ARRAY		=RW|TEXTURE_3D|ARRAY   
			,COUNT  
		};
		inline ShaderResourceType operator|(ShaderResourceType a, ShaderResourceType b)
		{
			return static_cast<ShaderResourceType>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
		}
		inline ShaderResourceType operator&(ShaderResourceType a, ShaderResourceType b)
		{
			return static_cast<ShaderResourceType>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
		}
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
			case D_24_UNORM_S_8_UINT:
				return sizeof(unsigned int);
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
			case D_24_UNORM_S_8_UINT:
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
