#pragma once
#include "Simul/Base/MemoryInterface.h"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include <vector>
#include <string>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif


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
		enum class ShaderResourceType;
		/// Base class for a view of a texture (i.e. for shaders to use). TextureView instances should not be created, except inside derived classes of crossplatform::Texture.
		class SIMUL_CROSSPLATFORM_EXPORT TextureView
		{
			ShaderResourceType type;
		};
		/// A Texture base class.
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		protected:
			bool cubemap;
			std::string name;
		public:
			Texture(const char *name=NULL);
			virtual ~Texture();
			void SetName(const char *n);
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual void LoadTextureArray(RenderPlatform *r,const std::vector<std::string> &texture_files)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual sce::Gnm::Texture *AsGnmTexture(int=-1){return 0;}
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView(int =-1){return 0;}
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int =0){return 0;}
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(){return 0;}
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(){return 0;}
			virtual bool HasRenderTargets() const{return (num_rt>0);}
			virtual bool IsComputable() const=0;
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(int =-1){return 0;}
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
			//! Initialize as an array texture if necessary. Returns true if the texture was initialized, or false if it was already in the required format.
			virtual bool ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false)=0;
			//! Initialize as a volume texture.
			virtual bool ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false)=0;
			//! Generate the mipmaps automatically.
			virtual void GenerateMips(DeviceContext &deviceContext)=0;
			//! Set the texture data from CPU memory.
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			//! Activate as a rendertarget - must call deactivateRenderTarget afterwards.
			virtual void activateRenderTarget(DeviceContext &deviceContext,int array_index=-1)=0;
			//! Deactivate as a rendertarget.
			virtual void deactivateRenderTarget(DeviceContext &deviceContext)=0;
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
			int GetArraySize() const
			{
				return arraySize;
			}
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			virtual bool IsCubemap() const;
			virtual void copyToMemory(DeviceContext &deviceContext,void *target,int start_texel,int num_texels)=0;
			int width,length,depth,arraySize,dim,mips;
			PixelFormat pixelFormat;
			RenderPlatform *renderPlatform;
			int num_rt;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
