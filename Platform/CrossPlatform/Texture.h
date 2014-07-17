#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
typedef unsigned GLuint;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture()
				:width(0)
				,length(0)
				,depth(0)
				,dim(0){}
			virtual ~Texture();
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView()=0;
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()=0;
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(){return 0;}
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(){return 0;}
			virtual GLuint AsGLuint()=0;
			virtual void ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,PixelFormat f,bool computable=false,bool rendertarget=false,int num_samples=1,int aa_quality=0)=0;
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
		};
	}
}