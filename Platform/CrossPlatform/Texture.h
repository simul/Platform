#pragma once
#include "Simul/Base/MemoryInterface.h"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11SamplerState;
typedef unsigned GLuint;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		struct SamplerStateDesc
		{
			enum Wrapping
			{
				WRAP,CLAMP,MIRROR
			};
			enum Filtering
			{
				POINT,LINEAR
			};
			Wrapping x,y,z;
			Filtering filtering;
		};
		class SIMUL_CROSSPLATFORM_EXPORT SamplerState
		{
		public:
			virtual ~SamplerState();
			virtual ID3D11SamplerState *asD3D11SamplerState()
			{
				return NULL;
			}
		};
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture()
				:width(0)
				,length(0)
				,depth(0)
				,dim(0)
	,pixelFormat(crossplatform::UNKNOWN){}
			virtual ~Texture();
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView(){return 0;}
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(){return 0;}
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(){return 0;}
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(){return 0;}
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(){return 0;}
			PixelFormat GetFormat() const
			{
				return pixelFormat;
			}
			virtual void ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0)=0;
			virtual void ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,PixelFormat f,bool computable)=0;
			virtual void ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1)=0;
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			virtual void activateRenderTarget(DeviceContext &deviceContext)=0;
			virtual void deactivateRenderTarget()=0;
			virtual int GetLength() const=0;
			virtual int GetWidth() const=0;
			virtual int GetDimension() const=0;
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			virtual void copyToMemory(DeviceContext &deviceContext,void *target,int start_texel,int num_texels)=0;
			int width,length,depth,dim;
			PixelFormat pixelFormat;
		};
	}
}