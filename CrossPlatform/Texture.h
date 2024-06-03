#pragma once
#include "Platform/Core/MemoryInterface.h"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/Shaders/SL/CppSl.sl"
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
namespace vk
{
	class Image;
	class ImageView;
	class Sampler;
}
namespace nvn
{
	class Texture;
	class TextureView;
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
		class Texture;
		struct DeviceContext;
		struct GraphicsDeviceContext;
		enum DepthComparison
		{
			DEPTH_NEVER=0,
			DEPTH_ALWAYS,
			DEPTH_LESS,
			DEPTH_EQUAL,
			DEPTH_LESS_EQUAL,
			DEPTH_GREATER,
			DEPTH_NOT_EQUAL,
			DEPTH_GREATER_EQUAL
		};
		enum class CompressionFormat : uint32_t
		{
			UNCOMPRESSED,
			BC1,
			BC3,
			BC4,
			BC5,
			ETC1,
			ETC2,
			PVRTC1_4_OPAQUE_ONLY,
			BC7_M6_OPAQUE_ONLY,
			BC6H
		};
		struct SamplerStateDesc
		{
			enum Wrapping
			{
				WRAP,CLAMP,MIRROR
			};
			//TODO: Add Mipmap mode for min filtering - AJR
			enum Filtering
			{
				POINT,LINEAR,ANISOTROPIC
			};
			Wrapping x,y,z;
			Filtering filtering;
			DepthComparison depthComparison;
			int slot;			// register slot
		};

		enum class VideoTextureType
		{
			NONE = 0,
			ENCODE,
			DECODE,
			PROCESS
		};

		/// A structure for creating or initializing textures.
		struct TextureCreate
		{
			void *external_texture=nullptr;
			int w=0;
			int l=0;
			int d=1;
			int arraysize=1;
			int mips = 1;
			bool cubemap=false;
			PixelFormat f=PixelFormat::UNKNOWN;
			bool make_rt=false;
			bool setDepthStencil=false;
			VideoTextureType vidTexType = VideoTextureType::NONE;
			bool computable = true;
			int numOfSamples=1;
			int aa_quality = 0;
			vec4 clear;
			float clearDepth=0.0f;
			uint clearStencil = 0;
			CompressionFormat compressionFormat= CompressionFormat::UNCOMPRESSED;
			const void* initialData = nullptr;
			bool cpuWritable = false;
		};
		//! A crossplatform viewport structure.
		struct Viewport
		{
			int x,y;
			int w,h;
			inline const Viewport &operator=(const int4 &i)
			{
				x=i.x;
				y=i.y;
				w=i.z;
				h=i.w;
				return *this;
			}
		};
		typedef void ApiRenderTarget;
		typedef void ApiDepthRenderTarget;
		//! Stores information about the current render targets
		struct SIMUL_CROSSPLATFORM_EXPORT TargetsAndViewport
		{
			struct TextureTarget
			{
				TextureTarget():texture(nullptr),layer(0),mip(0)
				{
				}
				Texture *texture;
				int layer;
				int mip;
			};
			TargetsAndViewport():temp(false),num(0),m_dt(nullptr),depthFormat(UNKNOWN)
			{
                for (int i = 0; i < 8; i++)
				{
					m_rt[i] = nullptr;
					rtFormats[i] = UNKNOWN;
				}
			}
			bool                        temp;
			int                         num;
            //! API pointer to the target
			const ApiRenderTarget*      m_rt[8];
			//! If using Simul textures, the targets, including layer and mip specification.
			TextureTarget				textureTargets[8];
			TextureTarget				depthTarget;
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
			virtual vk::Sampler *AsVulkanSampler()
			{
				return nullptr;
			}
			int default_slot;
			crossplatform::RenderPlatform *renderPlatform;
		};
		enum class ShaderResourceType : uint16_t;

		struct SIMUL_CROSSPLATFORM_EXPORT SubresourceRange
		{
			uint32_t	baseMipLevel = 0;
			uint32_t	mipLevelCount = -1;
			uint32_t	baseArrayLayer = 0;
			uint32_t	arrayLayerCount = -1;

			SubresourceRange() = default;
			SubresourceRange(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount)
				: baseMipLevel(baseMip), mipLevelCount(mipCount), baseArrayLayer(baseLayer), arrayLayerCount(layerCount) {}
			SubresourceRange(int32_t baseMip, int32_t mipCount, int32_t baseLayer, int32_t layerCount)
				: baseMipLevel((uint32_t)baseMip), mipLevelCount((uint32_t)mipCount), baseArrayLayer((uint32_t)baseLayer), arrayLayerCount((uint32_t)layerCount) {}
		};
		struct SIMUL_CROSSPLATFORM_EXPORT SubresourceLayers
		{
			uint32_t	mipLevel = 0;
			uint32_t	baseArrayLayer = 0;
			uint32_t	arrayLayerCount = -1;

			SubresourceLayers() = default;
			SubresourceLayers(uint32_t mip, uint32_t baseLayer, uint32_t layerCount)
				: mipLevel(mip), baseArrayLayer(baseLayer), arrayLayerCount(layerCount) {}
			SubresourceLayers(int32_t mip, int32_t baseLayer, int32_t layerCount)
				: mipLevel((uint32_t)mip), baseArrayLayer((uint32_t)baseLayer), arrayLayerCount((uint32_t)layerCount) {}
		};

		class Texture;
		/// Base class for a view of a texture (i.e. for shaders to use). TextureView instances should not be created, except inside derived classes of crossplatform::Texture.
		struct SIMUL_CROSSPLATFORM_EXPORT TextureView
		{
			ShaderResourceType	type				= ShaderResourceType::UNKNOWN;
			SubresourceRange	subresourceRange	= {};

			TextureView() = default;
			inline uint64_t GetHash() const 
			{
				return uint64_t((uint32_t)type) << 32
					| uint64_t((uint8_t)subresourceRange.baseMipLevel) << 24
					| uint64_t((uint8_t)subresourceRange.mipLevelCount) << 16
					| uint64_t((uint8_t)subresourceRange.baseArrayLayer) << 8
					| uint64_t((uint8_t)subresourceRange.arrayLayerCount) << 0;
			}
		};

		/// A Texture base class.
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture(const char *name=NULL);
			virtual ~Texture();
			virtual void SetName(const char *n);
			const char* GetName() const;
			/// Set the fence on this texture: it cannot be used until the fence has been triggered by the rendering API.
			void SetFence(DeviceContext &,unsigned long long);
			/// Get the current fence on this texture; it should not be used until the API has passed this fence.
			unsigned long long GetFence(DeviceContext &) const;
			/// Clear the fence: this texture is ok to use now.
			void ClearFence(DeviceContext &deviceContext);
			/// For API's that care about Resource State, aka Layout, tell the Simul API what state it was in to begin with.
			virtual void StoreExternalState(crossplatform::ResourceState){}
			/// For API's that care about Resource State, aka Layout, restore the state internally.
			virtual void RestoreExternalTextureState(DeviceContext &) {}
			virtual void SwitchToContext(DeviceContext &) {}
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
			bool ShouldGenerateMips() const
			{
				return shouldGenerateMips;
			}
			virtual void LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8,bool gen_mips=false)=0;
			virtual void Load3DTextureFromRawDataFile(crossplatform::DeviceContext& deviceContext, const char* pFilePathUtf8, crossplatform::TextureCreate* tc) { return; };
			virtual void LoadTextureArray(RenderPlatform *r,const std::vector<std::string> &texture_files,bool gen_mips=false)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects();
			virtual nvn::Texture* AsNVNTexture() { return 0; }
			virtual nvn::TextureView* AsNVNTextureView(const crossplatform::TextureView& textureView) { return 0; }
			//! Returns the GnmTexture specified by layer,mip. Default values of -1 mean "all".
			virtual sce::Gnm::Texture *AsGnmTexture(const crossplatform::TextureView& textureView){return 0;}
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11Resource *AsD3D11Resource(){return 0;}
			virtual ID3D12Resource *AsD3D12Resource() { return 0; }
			//! Returns the SRV specified by layer,mip. The type t ensures that the assigned resource is compatible (UNKNWON matches anything).
			//! Layer -1 means all layers at the given mip, while mip -1 defaults to the whole texture/layer.
			virtual ID3D11ShaderResourceView* AsD3D11ShaderResourceView(const crossplatform::TextureView& textureView) { return 0; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView, bool setState = true, bool pixelShader = true) { return 0; }
			//! Returns the UAV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView(const crossplatform::TextureView& textureView){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView) { return 0; }

			virtual ID3D11DepthStencilView *AsD3D11DepthStencilView(const crossplatform::TextureView& textureView){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12DepthStencilView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView) { return 0; }
			//! Returns the RTV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11RenderTargetView *AsD3D11RenderTargetView(const crossplatform::TextureView& textureView){return 0;}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE *AsD3D12RenderTargetView(crossplatform::DeviceContext &deviceContext, const crossplatform::TextureView& textureView) { return 0; }
			virtual bool HasRenderTargets() const { return renderTarget; }
			virtual bool IsComputable() const { return computable; }
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(int =-1, int = -1){return 0;}
			virtual vk::Image* AsVulkanImage(){return nullptr;}
			virtual vk::ImageView *AsVulkanImageView(const crossplatform::TextureView& textureView){return nullptr;}
			//! Get the crossplatform pixel format.
			PixelFormat GetFormat() const
			{
				return pixelFormat;
			}
			//! Initialize this object as a wrapper around a native, platform-specific texture. The interpretations of t is platform-dependent. Returns true if successful.
			virtual bool InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,int w=0,int l=0,PixelFormat f=PixelFormat::UNKNOWN,bool make_rt=false, bool setDepthStencil=false, int numOfSamples = 1)=0;
			virtual bool InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const TextureCreate *textureCreate);
			virtual bool InitFromExternalTexture3D(crossplatform::RenderPlatform*, void*, bool = false) { return false; }
			virtual bool EnsureTexture(RenderPlatform *, TextureCreate*);
			//! Initialize as a standard 2D texture. Not all platforms need \a wrap to be specified. Returns true if modified, false otherwise.
			virtual bool ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int m
				,PixelFormat f,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0,bool wrap=false,
				vec4 clear = vec4(0.0f, 0.0f, 0.0f, 0.0f), float clearDepth = 0.0f, uint clearStencil = 0)=0;
			// Create texture for use as a reference frame in video encoding or decoding.
			virtual bool ensureVideoTexture(RenderPlatform* renderPlatform, int w, int l, PixelFormat f, VideoTextureType texType) { return true; };
			//! Initialize as an array texture if necessary. Returns true if the texture was initialized, or false if it was already in the required format.
			virtual bool ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,int mips,PixelFormat f,bool computable=false,bool rendertarget=false,bool cubemap=false)=0;
			//! Initialize as a volume texture.
			virtual bool ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false)=0;
			//! Clear the depth stencil
			virtual void ClearDepthStencil(GraphicsDeviceContext &deviceContext, float = 0, int = 0) = 0;
			//! Generate the mipmaps automatically.
			virtual void GenerateMips(GraphicsDeviceContext &deviceContext)=0;
			//! Set the texture data from CPU memory.
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			//! Activate as a rendertarget - must call deactivateRenderTarget afterwards.
			virtual void activateRenderTarget(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::TextureView textureView = crossplatform::TextureView());
			//! Deactivate as a rendertarget.
			virtual void deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext);
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
			virtual void FinishLoading(DeviceContext & ) {}
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
			virtual ShaderResourceType GetShaderResourceTypeForRTVAndDSV();
			int width,length,depth,arraySize,dim,mips;
			PixelFormat pixelFormat= PixelFormat::UNKNOWN;
			RenderPlatform *renderPlatform=nullptr;
			bool textureLoadComplete = false;
		protected:
			bool cubemap = false;
			bool computable=false;
			bool renderTarget = false;
			bool depthStencil = false;
			bool external_texture = false;
			bool shouldGenerateMips = false;
			std::string name;
			simul::crossplatform::TargetsAndViewport targetsAndViewport;
			// For API's that don't track resources:
			bool unfenceable = false;
			// a wrapper around stbi_load_from_memory.
			bool TranslateLoadedTextureData(void *&target,const void *src,size_t size,int &x,int &y,int &num_channels,int req_num_channels);
			void FreeTranslatedTextureData(void *data);
			bool ValidateTextureView(const TextureView& textureView);
		private:
			bool stbi_loaded = false;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
