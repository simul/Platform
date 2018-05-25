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
struct ID3D11Resource;
struct ID3D11SamplerState;
typedef unsigned GLuint;
extern "C"
{
struct ID3D12Resource;
}

struct D3D12_CPU_DESCRIPTOR_HANDLE;

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
				POINT,LINEAR,ANISOTROPIC
			};
			Wrapping x,y,z;
			Filtering filtering;
			int slot;			// register slot
		};
		//! A crossplatform viewport structure.
		struct Viewport
		{
			int x,y;
			int w,h;
			float znear,zfar;
		};
		typedef void ApiRenderTarget;
		typedef void ApiDepthRenderTarget;
		//! Stores information about the current render targets
		struct SIMUL_CROSSPLATFORM_EXPORT TargetsAndViewport
		{
			TargetsAndViewport():temp(false),num(0),m_dt(nullptr),depthFormat(UNKNOWN)
			{
                for (int i = 0; i < 8; i++) { m_rt[i] = nullptr; rtFormats[i] = UNKNOWN; }
			}
			bool                        temp;
			int                         num;
            //! API pointer to the target
			const ApiRenderTarget*      m_rt[8];
            //! The pixel format of the target
            PixelFormat                 rtFormats[8];
            //! If any, the API pointer to the depth surface
			const ApiDepthRenderTarget* m_dt;
            //! Depth format
            PixelFormat                 depthFormat;
			crossplatform::Viewport     viewport;
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
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12SamplerState()
			{
				return NULL;
			}
			virtual const sce::Gnm::Sampler *AsGnmSampler() const {return 0;}
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
		public:
			ShaderResourceType type;
		};
		/// A Texture base class.
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		protected:
			bool cubemap;
			bool external_texture;
			bool depthStencil;
			std::string name;
			simul::crossplatform::TargetsAndViewport targetsAndViewport;
			// For API's that don't track resources:
			unsigned long long fence;
			bool unfenceable;
		public:
			Texture(const char *name=NULL);
			virtual ~Texture();
			virtual void SetName(const char *n);
			/// Set the fence on this texture: it cannot be used until the fence has been triggered by the rendering API.
			void SetFence(unsigned long long);
			/// Get the current fence on this texture; it should not be used until the API has passed this fence.
			unsigned long long GetFence() const;
			/// Clear the fence: this texture is ok to use now.
			void ClearFence();
			/// Is the texture "unfenceable": if so, it need never be checked for fences, either because it is constant,
			/// or because we don't care if it's not been updated.
			bool IsUnfenceable() const
			{
				return unfenceable;
			}
			/// Set whether to never check for fences on this texture.
			void SetUnfenceable(bool v)
			{
				unfenceable=v;
			}
			/// Get whether texture is a depth stencil
			bool IsDepthStencil() const
			{
				return depthStencil;
			}
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8)=0;
			virtual void LoadTextureArray(RenderPlatform *r,const std::vector<std::string> &texture_files,int specify_mips=-1)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			//! Returns the GnmTexture specified by layer,mip. Default values of -1 mean "all".
			virtual sce::Gnm::Texture *AsGnmTexture(crossplatform::ShaderResourceType =crossplatform::ShaderResourceType::UNKNOWN,int=-1,int=-1){return 0;}
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11Resource *AsD3D11Resource(){return 0;}
			virtual ID3D12Resource *AsD3D12Resource() { return 0; }
			//! Returns the SRV specified by layer,mip. The type t ensures that the assigned resource is compatible (UNKNWON matches anything).
			//! Layer -1 means all layers at the given mip, while mip -1 defaults to the whole texture/layer.
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView(crossplatform::ShaderResourceType =crossplatform::ShaderResourceType::UNKNOWN,int=-1,int=-1){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12ShaderResourceView(bool = true,crossplatform::ShaderResourceType = crossplatform::ShaderResourceType::UNKNOWN, int = -1, int = -1) { return 0; }
			//! Returns the UAV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(int=-1,int=-1){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12UnorderedAccessView(int = -1, int = -1) { return 0; }
			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12DepthStencilView() { return 0; }
			//! Returns the RTV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(int=-1,int=-1){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12RenderTargetView(int = -1, int = -1) { return 0; }
			virtual bool HasRenderTargets() const {return 0;}
			virtual bool IsComputable() const=0;
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(int =-1, int = -1){return 0;}
			//! Get the crossplatform pixel format.
			PixelFormat GetFormat() const
			{
				return pixelFormat;
			}
			//! Initialize this object as a wrapper around a native, platform-specific texture. The interpretations of t and srv are platform-dependent.
			virtual void InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,bool make_rt=false, bool setDepthStencil=false)=0;
			virtual void InitFromExternalTexture3D(crossplatform::RenderPlatform *,void *,void *,bool =false) {}
			//! Initialize as a standard 2D texture. Not all platforms need \a wrap to be specified. Returns true if modified, false otherwise.
			virtual bool ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l
				,PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0,bool wrap=false,
				vec4 clear = vec4(0.5f, 0.5f, 0.2f, 1.0f), float clearDepth = 1.0f, uint clearStencil = 0)=0;
			//! Initialize as an array texture if necessary. Returns true if the texture was initialized, or false if it was already in the required format.
			virtual bool ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,int mips,PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false)=0;
			//! Initialize as a volume texture.
			virtual bool ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false)=0;
			//! Clear the depth stencil
			virtual void ClearDepthStencil(DeviceContext &deviceContext, float = 0, int = 0) = 0;
			//! Generate the mipmaps automatically.
			virtual void GenerateMips(DeviceContext &deviceContext)=0;
			//! Set the texture data from CPU memory.
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			//! Activate as a rendertarget - must call deactivateRenderTarget afterwards.
			virtual void activateRenderTarget(DeviceContext &deviceContext,int array_index=-1,int mip_index=0);
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
			//! The number of "faces": either equal to the array size, or in the case of a cubemap, six times that number.
			int NumFaces() const
			{
				return cubemap?arraySize*6:arraySize;
			}
			virtual void copyToMemory(DeviceContext &deviceContext,void *target,int start_texel,int num_texels)=0;
			int width,length,depth,arraySize,dim,mips;
			PixelFormat pixelFormat;
			RenderPlatform *renderPlatform;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
