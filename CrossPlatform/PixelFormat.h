#pragma once
namespace simul
{
	namespace crossplatform
	{
		enum class ResourceState: unsigned short
		{
			COMMON	= 0,
			VERTEX_AND_CONSTANT_BUFFER	= 0x1,
			INDEX_BUFFER	= 0x2,
			RENDER_TARGET	= 0x4,
			UNORDERED_ACCESS	= 0x8,
			DEPTH_WRITE	= 0x10,
			DEPTH_READ	= 0x20,
			NON_PIXEL_SHADER_RESOURCE	= 0x40,
			PIXEL_SHADER_RESOURCE	= 0x80,
			STREAM_OUT	= 0x100,
			INDIRECT_ARGUMENT	= 0x200,
			COPY_DEST	= 0x400,
			COPY_SOURCE	= 0x800,
			RESOLVE_DEST	= 0x1000,
			RESOLVE_SOURCE	= 0x2000,
			SHADER_RESOURCE	= ( 0x80 | 0x40 )  ,
			GENERAL_READ	= ( ( ( ( ( 0x1 | 0x2 )  | 0x40 )  | 0x80 )  | 0x200 )  | 0x800 ) ,
			UNKNOWN=0xFFFF
		};
		//! A cross-platform equivalent to the OpenGL and DirectX pixel formats
		enum PixelFormat
		{
			UNKNOWN=0
			,RGBA_32_FLOAT
			,RGBA_32_UINT
			,RGBA_32_INT
			,RGBA_16_FLOAT
			,RGBA_16_UINT
			,RGBA_16_INT
			,RGBA_16_SNORM
			,RGBA_16_UNORM
			,RGBA_8_UINT
			,RGBA_8_INT
			,RGBA_8_SNORM
			,RGBA_8_UNORM
			,RGBA_8_UNORM_COMPRESSED
			,RGBA_8_UNORM_SRGB
			,BGRA_8_UNORM

			,RGB_32_FLOAT
			,RGB_32_UINT
			,RGB_16_FLOAT
			,RGB_10_A2_UINT
			,RGB_10_A2_INT
			,RGB_11_11_10_FLOAT
			,RGB_8_UNORM
			,RGB_8_SNORM

			,RG_32_FLOAT
			,RG_32_UINT
			,RG_16_FLOAT
			,RG_16_UINT
			,RG_16_INT
			,RG_8_UNORM
			,RG_8_SNORM

			,R_32_FLOAT
			,R_32_FLOAT_X_8
			,LUM_32_FLOAT
			,INT_32_FLOAT
			,R_32_UINT
			,R_32_INT

			// depth formats:
			,D_32_FLOAT// DXGI_FORMAT_D32_FLOAT or GL_DEPTH_COMPONENT32F
			,D_32_UINT
			,D_32_FLOAT_S_8_UINT
			,D_24_UNORM_S_8_UINT
			,D_16_UNORM

			,R_16_FLOAT
			,R_8_UNORM
			,R_8_SNORM

			,RGB10_A2_UNORM

			,R_24_FLOAT_X_8

			,NV12
		};
		//! Pixel formats for pixel shader output - only relevant for some API's.
		enum PixelOutputFormat
		{
			FMT_UNKNOWN
			,FMT_32_GR
			,FMT_32_AR 
			,FMT_FP16_ABGR 
			,FMT_UNORM16_ABGR 
			,FMT_SNORM16_ABGR 
			,FMT_UINT16_ABGR 
			,FMT_SINT16_ABGR 
			,FMT_32_ABGR 
			,OUTPUT_FORMAT_COUNT
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
			,ACCELERATION_STRUCTURE		=32768
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
		inline ShaderResourceType operator^(ShaderResourceType a, ShaderResourceType b)
		{
			return static_cast<ShaderResourceType>(static_cast<unsigned int>(a) ^ static_cast<unsigned int>(b));
		}
		inline ShaderResourceType operator~(ShaderResourceType a)
		{
			return static_cast<ShaderResourceType>(~static_cast<unsigned int>(a));
		}
		inline ShaderResourceType& operator|=(ShaderResourceType& a, ShaderResourceType b)
		{
			a = static_cast<ShaderResourceType>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
			return a; 
		}
		inline ShaderResourceType& operator&=(ShaderResourceType& a, ShaderResourceType b)
		{
			a = static_cast<ShaderResourceType>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
			return a;
		}
		inline ShaderResourceType& operator^=(ShaderResourceType& a, ShaderResourceType b)
		{
			a = static_cast<ShaderResourceType>(static_cast<unsigned int>(a) ^ static_cast<unsigned int>(b));
			return a;
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
			case RGBA_16_FLOAT:
			case RGB_16_FLOAT:
			case RG_16_FLOAT:
			case R_16_FLOAT:
			case D_16_UNORM:
				return sizeof(short);
			case D_24_UNORM_S_8_UINT:
				return sizeof(unsigned int);
			case RGBA_8_UNORM:
			case BGRA_8_UNORM:
			case RGBA_8_UNORM_SRGB:
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
			case BGRA_8_UNORM:
			case RGBA_8_UNORM_SRGB:
			case RGBA_8_SNORM:
				return 4;
			case RGB_32_FLOAT:
			case RGB_32_UINT:
			case RGB_16_FLOAT:
				return 3;
			case RG_32_FLOAT:
			case RG_32_UINT:
			case RG_16_FLOAT:
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
		extern PixelFormat TypeToFormat(const char *txt);
	}
}
