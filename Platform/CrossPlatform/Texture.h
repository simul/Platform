#pragma once
#include "Simul/Base/MemoryInterface.h"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include <vector>
#include <string>

namespace sce
{
	namespace Gnm
	{
		class Texture;
		class Sampler;
	}
}

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
			int slot;			// register slot
		};
		class SIMUL_CROSSPLATFORM_EXPORT SamplerState
		{
		public:
			SamplerState();
			virtual ~SamplerState();
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11SamplerState *asD3D11SamplerState()
			{
				return NULL;
			}
			virtual sce::Gnm::Sampler *AsGnmSampler(){return 0;}
			virtual GLuint asGLuint()
			{
				return 0;
			}
			int default_slot;
		};
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture()
				:width(0)
				,length(0)
				,depth(0)
				,dim(0)
				,mips(1)
				,pixelFormat(crossplatform::UNKNOWN)
				,num_rt(0)
			,renderPlatform(NULL){}
			virtual ~Texture();
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual void LoadTextureArray(RenderPlatform *r,const std::vector<std::string> &texture_files)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual sce::Gnm::Texture *AsGnmTexture(){return 0;}
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView(){return 0;}
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int =0){return 0;}
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(){return 0;}
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(){return 0;}
			virtual bool HasRenderTargets() const{return (num_rt>0);}
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(){return 0;}
			//! Get the crossplatform pixel format.
			PixelFormat GetFormat() const
			{
				return pixelFormat;
			}
			//! Initialize this object as a wrapper around a native, platform-specific texture. The interpretations of t and srv are platform-dependent.
			virtual void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false)=0;
			//! Initialize as a standard 2D texture. Not all platforms need \a wrap to be specified.
			virtual void ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0,bool wrap=false)=0;
			//! Initialize as an array texture.
			virtual void ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false)=0;
			//! Initialize as a volume texture.
			virtual void ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false)=0;
			//! Generate the mipmaps automatically.
			virtual void GenerateMips(DeviceContext &deviceContext)=0;
			//! Set the texture data from CPU memory.
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			//! Activate as a rendertarget - must call deactivateRenderTarget afterwards.
			virtual void activateRenderTarget(DeviceContext &deviceContext)=0;
			//! Deactivate as a rendertarget.
			virtual void deactivateRenderTarget()=0;
			virtual int GetLength() const
			{
				return length;
			}
			virtual int GetWidth() const
			{
				return width;
			}
			virtual int GetDepth() const
			{
				return depth;
			}
			virtual int GetDimension() const
			{
				return dim;
			}
			int GetMipCount() const
			{
				return mips;
			}
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			virtual void copyToMemory(DeviceContext &deviceContext,void *target,int start_texel,int num_texels)=0;
			int width,length,depth,dim,mips;
			PixelFormat pixelFormat;
			RenderPlatform *renderPlatform;
			int num_rt;
		};
	}
}